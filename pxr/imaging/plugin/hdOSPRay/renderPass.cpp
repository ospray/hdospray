//
// Copyright 2018 Intel
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/glf/glew.h"

#include "pxr/imaging/hdOSPRay/renderParam.h"
#include "pxr/imaging/hdOSPRay/renderPass.h"

#include "pxr/imaging/hdOSPRay/config.h"
#include "pxr/imaging/hdOSPRay/context.h"
#include "pxr/imaging/hdOSPRay/mesh.h"
#include "pxr/imaging/hdOSPRay/renderDelegate.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/renderPassState.h"

#include "pxr/base/gf/vec2f.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

inline float
rad(float deg)
{
    return deg * M_PI / 180.0f;
}

HdOSPRayRenderPass::HdOSPRayRenderPass(
       HdRenderIndex* index, HdRprimCollection const& collection,
       OSPModel model, OSPRenderer renderer, std::atomic<int>* sceneVersion,
       std::shared_ptr<HdOSPRayRenderParam> renderParam)
    : HdRenderPass(index, collection)
    , _pendingResetImage(false)
    , _pendingModelUpdate(true)
    , _renderer(renderer)
    , _sceneVersion(sceneVersion)
    , _lastRenderedVersion(0)
    , _lastRenderedModelVersion(0)
    , _width(0)
    , _height(0)
    , _model(model)
    , _inverseViewMatrix(1.0f) // == identity
    , _inverseProjMatrix(1.0f) // == identity
    , _clearColor(0.0707f, 0.0707f, 0.0707f)
    , _renderParam(renderParam)
{
    _camera = ospNewCamera("perspective");
    std::vector<OSPLight> lights;
    auto ambient = ospNewLight(_renderer, "ambient");
    ospSet3f(ambient, "color", 1.f, 1.f, 1.f);
    ospSet1f(ambient, "intensity", 0.35f);
    ospCommit(ambient);
    lights.push_back(ambient);
    auto sun = ospNewLight(_renderer, "DirectionalLight");
    ospSet3f(sun, "color", 1.f, 232.f / 255.f, 166.f / 255.f);
    ospSet3f(sun, "direction", 0.562f, -0.25f, -0.25f);
    ospSet1f(sun, "intensity", 3.3f);
    ospCommit(sun);
    lights.push_back(sun);
    auto bounce = ospNewLight(_renderer, "DirectionalLight");
    ospSet3f(bounce, "color", 127.f / 255.f, 178.f / 255.f, 255.f / 255.f);
    ospSet3f(bounce, "direction", -0.13f, -.94f, -.105f);
    ospSet1f(bounce, "intensity", 0.95f);
    ospCommit(bounce);
    lights.push_back(bounce);

    OSPData lightArray = ospNewData(lights.size(), OSP_OBJECT, &(lights[0]));
    ospSetData(_renderer, "lights", lightArray);
    ospRelease(lightArray);
    ospSet4f(_renderer, "bgColor", _clearColor[0], _clearColor[1],
             _clearColor[2], 1.f);

    ospSetObject(_renderer, "model", _model);
    ospSetObject(_renderer, "camera", _camera);

    ospSet1i(_renderer, "maxDepth", 8);
    ospSet1f(_renderer, "aoDistance", 15.0f);
    ospSet1i(_renderer, "shadowsEnabled", true);
    ospSet1f(_renderer, "maxContribution", 2.f);
    ospSet1f(_renderer, "minContribution", 0.05f);
    ospSet1f(_renderer, "epsilon", 0.001f);
    ospSet1i(_renderer, "useGeometryLights", 0);

    ospCommit(_renderer);

#if HDOSPRAY_ENABLE_DENOISER
    _denoiserDevice = oidn::newDevice();
    _denoiserDevice.commit();
    _denoiserFilter = _denoiserDevice.newFilter("RT");
#endif
}

HdOSPRayRenderPass::~HdOSPRayRenderPass()
{
}

void
HdOSPRayRenderPass::ResetImage()
{
    // Set a flag to clear the sample buffer the next time Execute() is called.
    _pendingResetImage = true;
}

bool
HdOSPRayRenderPass::IsConverged() const
{
    // A super simple heuristic: consider ourselves converged after N
    // samples. Since we currently uniformly sample the framebuffer, we can
    // use the sample count from pixel(0,0).
    unsigned int samplesToConvergence
           = HdOSPRayConfig::GetInstance().samplesToConvergence;
    return ((unsigned int)_numSamplesAccumulated >= samplesToConvergence);
}

void
HdOSPRayRenderPass::_MarkCollectionDirty()
{
    // If the drawable collection changes, we should reset the sample buffer.
    _pendingResetImage = true;
    _pendingModelUpdate = true;
}

void
HdOSPRayRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                             TfTokenVector const& renderTags)
{
    HdRenderDelegate* renderDelegate = GetRenderIndex()->GetRenderDelegate();

    float aspect = _width / float(_height);
    ospSetf(_camera, "aspect", aspect);
    GfVec3f origin = GfVec3f(0, 0, 0);
    GfVec3f dir = GfVec3f(0, 0, -1);
    GfVec3f up = GfVec3f(0, 1, 0);
    dir = _inverseProjMatrix.Transform(dir);
    origin = _inverseViewMatrix.Transform(origin);
    dir = _inverseViewMatrix.TransformDir(dir).GetNormalized();
    up = _inverseViewMatrix.TransformDir(up).GetNormalized();

    double prjMatrix[4][4];
    renderPassState->GetProjectionMatrix().Get(prjMatrix);
    float fov = 2.0 * std::atan(1.0 / prjMatrix[1][1]) * 180.0 / M_PI;

    ospSet3fv(_camera, "pos", &origin[0]);
    ospSet3fv(_camera, "dir", &dir[0]);
    ospSet3fv(_camera, "up", &up[0]);
    ospSetf(_camera, "fovy", fov);
    ospCommit(_camera);

    // XXX: Add collection and renderTags support.
    // update conig options
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    if (_lastSettingsVersion != currentSettingsVersion) {
        _samplesToConvergence = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->samplesToConvergence, 0);
        float aoDistance = renderDelegate->GetRenderSetting<float>(
               HdOSPRayRenderSettingsTokens->aoDistance, 0);
        _useDenoiser = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->useDenoiser, false);
        int spp = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->samplesPerFrame, 0);
        int aoSamples = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->ambientOcclusionSamples, 0);
        _ambientLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->ambientLight, false);
        _eyeLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->eyeLight, false);
        _keyLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->keyLight, false);
        _fillLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->fillLight, false);
        _backLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->backLight, false);
        if (spp != _spp || aoSamples != _aoSamples
            || aoDistance != aoDistance) {
            _spp = spp;
            _aoSamples = aoSamples;
            ospSet1i(_renderer, "spp", _spp);
            ospSet1i(_renderer, "aoSamples", _aoSamples);
            ospSet1f(_renderer, "aoDistance", _aoSamples);
            ospCommit(_renderer);
        }
        _lastSettingsVersion = currentSettingsVersion;
        ResetImage();
    }
    // XXX: Add clip planes support.

    // add lights
    GfVec3f right = GfCross(dir, up);
    float camDistance = 50.0f;
    float lightMultiplier = 100000.0f;
    std::vector<OSPLight> lights;
    if (_ambientLight) {
        auto ambient = ospNewLight(_renderer, "ambient");
        ospSet3f(ambient, "color", 1.f, 1.f, 1.f);
        ospSet1f(ambient, "intensity", 0.35f);
        ospCommit(ambient);
        lights.push_back(ambient);
    }
    if (_eyeLight) {
        auto eyeLight = ospNewLight3("directional");
        ospSet3f(eyeLight, "color", 1.f, 232.f / 255.f, 166.f / 255.f);
        ospSet3fv(eyeLight, "direction", &dir[0]);
        ospSet1f(eyeLight, "intensity", 3.3f);
        ospCommit(eyeLight);
        lights.push_back(eyeLight);
    }
    if (_keyLight) {
        auto keyLight = ospNewLight3("sphere");
        auto keyHorz = -1.0f / tan(rad(45.0f)) * right;
        auto keyVert = 1.0f / tan(rad(70.0f)) * up;
        auto keyPos
               = origin + (dir + keyVert / 3.f + keyHorz / 3.f) * camDistance;
        ospSet3f(keyLight, "color", .8f, .8f, .8f);
        ospSet3fv(keyLight, "position", &keyPos[0]);
        ospSet1f(keyLight, "intensity", 0.95f*lightMultiplier);
        ospSet1f(keyLight, "radius", 2.5f);
        ospCommit(keyLight);
        lights.push_back(keyLight);
    }
    if (_fillLight) {
        auto fillLight = ospNewLight(_renderer, "sphere");
        auto fillHorz = 1.0f / tan(rad(30.0f)) * right;
        auto fillVert = 1.0f / tan(rad(45.0f)) * up;
        auto fillPos = origin + (fillVert + fillHorz) * camDistance;
        ospSet3f(fillLight, "color", .6f, .6f, .6f);
        ospSet3fv(fillLight, "position", &fillPos[0]);
        ospSet1f(fillLight, "intensity", 0.95f*lightMultiplier);
        ospSet1f(fillLight, "radius", 2.5f);
        ospCommit(fillLight);
        lights.push_back(fillLight);
    }
    if (_backLight) {
        auto backLight = ospNewLight(_renderer, "sphere");
        auto backHorz = 1.0f / tan(rad(60.0f)) * right;
        auto backVert = -1.0f / tan(rad(60.0f)) * up;
        auto backPos = -1.f * origin + (backHorz + backVert) * camDistance;
        ospSet3f(backLight, "color", .6f, .6f, .6f);
        ospSet3fv(backLight, "position", &backPos[0]);
        ospSet1f(backLight, "intensity", 0.95f*lightMultiplier);
        ospSet1f(backLight, "radius", 2.5f);
        ospCommit(backLight);
        lights.push_back(backLight);
    }
    OSPData lightArray = ospNewData(lights.size(), OSP_OBJECT, &(lights[0]));
    ospSetData(_renderer, "lights", lightArray);
    ospRelease(lightArray);
    ospCommit(_renderer);

    // If the viewport has changed, resize the sample buffer.
    GfVec4f vp = renderPassState->GetViewport();
    if (_width != vp[2] || _height != vp[3]) {
        _width = vp[2];
        _height = vp[3];
        _frameBuffer = ospNewFrameBuffer(
               osp::vec2i({ (int)_width, (int)_height }), OSP_FB_RGBA32F,
               OSP_FB_COLOR | OSP_FB_ACCUM |
#if HDOSPRAY_ENABLE_DENOISER
                      OSP_FB_NORMAL | OSP_FB_ALBEDO |
#endif
                      0);
        ospCommit(_frameBuffer);
        _colorBuffer.resize(_width * _height);
        _normalBuffer.resize(_width * _height);
        _albedoBuffer.resize(_width * _height);
        _denoisedBuffer.resize(_width * _height);
        _pendingResetImage = true;
        _denoiserDirty = true;
    }

    int currentSceneVersion = _sceneVersion->load();
    if (_lastRenderedVersion != currentSceneVersion) {
        ResetImage();
        _lastRenderedVersion = currentSceneVersion;
    }

    if (_pendingModelUpdate) {
        ospCommit(_model);
        _pendingModelUpdate = false;
    }

    int currentModelVersion = _renderParam->GetModelVersion();
    if (_lastRenderedModelVersion != currentModelVersion) {
        ResetImage();
        _lastRenderedModelVersion = currentModelVersion;
    }

    // Update camera
    auto inverseViewMatrix
           = renderPassState->GetWorldToViewMatrix().GetInverse();
    auto inverseProjMatrix
           = renderPassState->GetProjectionMatrix().GetInverse();

    if (inverseViewMatrix != _inverseViewMatrix
        || inverseProjMatrix != _inverseProjMatrix) {
        ResetImage();
        _inverseViewMatrix = inverseViewMatrix;
        _inverseProjMatrix = inverseProjMatrix;
    }

    // Reset the sample buffer if it's been requested.
    if (_pendingResetImage) {
        ospFrameBufferClear(_frameBuffer, OSP_FB_ACCUM);
        _pendingResetImage = false;
        _numSamplesAccumulated = 0;
        if (_useDenoiser) {
            _spp = HdOSPRayConfig::GetInstance().samplesPerFrame;
            ospSet1i(_renderer, "spp", _spp);
            ospCommit(_renderer);
        }
    }

    // Render the frame
    ospRenderFrame(_frameBuffer, _renderer,
                   OSP_FB_COLOR | OSP_FB_ACCUM |
#if HDOSPRAY_ENABLE_DENOISER
                          OSP_FB_NORMAL | OSP_FB_ALBEDO |
#endif
                          0);
    _numSamplesAccumulated += std::max(1, _spp);

    // Resolve the image buffer: find the average color per pixel by
    // dividing the summed color by the number of samples;
    // and convert the image into a GL-compatible format.
    const void* rgba = ospMapFrameBuffer(_frameBuffer, OSP_FB_COLOR);
    memcpy((void*)&_colorBuffer[0], rgba, _width * _height * 4 * sizeof(float));
    ospUnmapFrameBuffer(rgba, _frameBuffer);
    if (_useDenoiser && _numSamplesAccumulated >= _denoiserSPPThreshold) {
        int newSPP
               = std::max((int)HdOSPRayConfig::GetInstance().samplesPerFrame, 1)
               * 4;
        if (_spp != newSPP) {
            ospSet1i(_renderer, "spp", _spp);
            ospCommit(_renderer);
        }
        Denoise();
    }

    // Blit!
    glDrawPixels(_width, _height, GL_RGBA, GL_FLOAT, &_colorBuffer[0]);
}

void
HdOSPRayRenderPass::Denoise()
{
    _denoisedBuffer = _colorBuffer;
#if HDOSPRAY_ENABLE_DENOISER
    const auto size = _width * _height;
    const osp::vec4f* rgba
           = (const osp::vec4f*)ospMapFrameBuffer(_frameBuffer, OSP_FB_COLOR);
    std::copy(rgba, rgba + size, _colorBuffer.begin());
    ospUnmapFrameBuffer(rgba, _frameBuffer);
    const osp::vec3f* normal
           = (const osp::vec3f*)ospMapFrameBuffer(_frameBuffer, OSP_FB_NORMAL);
    std::copy(normal, normal + size, _normalBuffer.begin());
    ospUnmapFrameBuffer(normal, _frameBuffer);
    const osp::vec3f* albedo
           = (const osp::vec3f*)ospMapFrameBuffer(_frameBuffer, OSP_FB_ALBEDO);
    std::copy(albedo, albedo + size, _albedoBuffer.begin());
    ospUnmapFrameBuffer(albedo, _frameBuffer);

    if (_denoiserDirty) {
        _denoiserFilter.setImage("color", (void*)_colorBuffer.data(),
                                 oidn::Format::Float3, _width, _height, 0,
                                 sizeof(osp::vec4f));

        _denoiserFilter.setImage("normal", (void*)_normalBuffer.data(),
                                 oidn::Format::Float3, _width, _height, 0,
                                 sizeof(osp::vec3f));

        _denoiserFilter.setImage("albedo", (void*)_albedoBuffer.data(),
                                 oidn::Format::Float3, _width, _height, 0,
                                 sizeof(osp::vec3f));

        _denoiserFilter.setImage("output", _denoisedBuffer.data(),
                                 oidn::Format::Float3, _width, _height, 0,
                                 sizeof(osp::vec4f));

        _denoiserFilter.set("hdr", true);
        _denoiserFilter.commit();
        _denoiserDirty = false;
    }

    _denoiserFilter.execute();
    _colorBuffer = _denoisedBuffer;
    // Carson: not sure we need two buffers
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE
