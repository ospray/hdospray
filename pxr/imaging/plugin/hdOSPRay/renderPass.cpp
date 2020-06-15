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

#include "ospray/ospray_util.h"

#include <iostream>

using namespace ospcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

inline float
rad(float deg)
{
    return deg * M_PI / 180.0f;
}

HdOSPRayRenderPass::HdOSPRayRenderPass(
       HdRenderIndex* index, HdRprimCollection const& collection,
       opp::Renderer renderer, std::atomic<int>* sceneVersion,
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
    , _inverseViewMatrix(1.0f) // == identity
    , _inverseProjMatrix(1.0f) // == identity
    , _clearColor(0.0f, 0.0f, 0.0f)
    , _renderParam(renderParam)
{
    _camera = opp::Camera("perspective");
    _renderer.setParam("backgroundColor", vec4f(_clearColor[0], _clearColor[1],
                _clearColor[2], 0.f));

    _renderer.setParam("maxDepth", 8);
    _renderer.setParam("aoDistance", 15.0f);
    _renderer.setParam("shadowsEnabled", true);
    _renderer.setParam("maxContribution", 2.f);
    _renderer.setParam("minContribution", 0.05f);
    _renderer.setParam("epsilon", 0.001f);
    _renderer.setParam("useGeometryLights", 0);

    _renderer.commit();

#if 0 //HDOSPRAY_ENABLE_DENOISER
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

    // If the viewport has changed, resize the sample buffer.
    GfVec4f vp = renderPassState->GetViewport();
    if (_width != vp[2] || _height != vp[3]) {
        _width = vp[2];
        _height = vp[3];
        _frameBuffer
               = opp::FrameBuffer(vec2i((int)_width, (int)_height), OSP_FB_RGBA32F,
                                   OSP_FB_COLOR | OSP_FB_ACCUM |
#if HDOSPRAY_ENABLE_DENOISER
                                          OSP_FB_NORMAL | OSP_FB_ALBEDO |
#endif
                                          0);
        if (_useDenoiser)
          _frameBuffer.setParam("imageOperation", opp::ImageOperation("denoise"));
        _frameBuffer.commit();
        _colorBuffer.resize(_width * _height);
        _pendingResetImage = true;
        _denoiserDirty = true;
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

    float aspect = _width / float(_height);
    _camera.setParam("aspect", aspect);
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

    _camera.setParam("position", vec3f(origin[0], origin[1], origin[2]));
    _camera.setParam("direction", vec3f(dir[0], dir[1], dir[2]));
    _camera.setParam("up", vec3f(up[0], up[1], up[2]));
    _camera.setParam("fovy", fov);
    _camera.commit();

    // XXX: Add collection and renderTags support.
    // update conig options
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    if (_lastSettingsVersion != currentSettingsVersion) {
        _samplesToConvergence = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->samplesToConvergence, 100);
        float aoDistance = renderDelegate->GetRenderSetting<float>(
               HdOSPRayRenderSettingsTokens->aoDistance, 10.f);
        _useDenoiser = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->useDenoiser, false);
        _pixelFilterType
               = (OSPPixelFilterTypes)renderDelegate->GetRenderSetting<int>(
                      HdOSPRayRenderSettingsTokens->pixelFilterType,
                      (int)OSPPixelFilterTypes::OSP_PIXELFILTER_GAUSS);
        int spp = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->samplesPerFrame, 1);
        int lSamples = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->lightSamples, -1);
        int aoSamples = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->ambientOcclusionSamples, 0);
        int maxDepth = renderDelegate->GetRenderSetting<int>(
               HdOSPRayRenderSettingsTokens->maxDepth, 5);
        _ambientLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->ambientLight, false);
        // default static ospray directional lights
        _staticDirectionalLights = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->staticDirectionalLights, false);
        // eye, key, fill, and back light are copied from USD GL (Storm)
        // defaults.
        _eyeLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->eyeLight, false);
        _keyLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->keyLight, false);
        _fillLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->fillLight, false);
        _backLight = renderDelegate->GetRenderSetting<bool>(
               HdOSPRayRenderSettingsTokens->backLight, false);
        if (spp != _spp || aoSamples != _aoSamples || aoDistance != aoDistance
            || maxDepth != _maxDepth || lSamples != _lightSamples) {
            _spp = spp;
            _aoSamples = aoSamples;
            _maxDepth = maxDepth;
            _lightSamples = lSamples;
            _renderer.setParam("pixelSamples", _spp);
            _renderer.setParam("lightSamples", _lightSamples);
            _renderer.setParam("aoSamples", _aoSamples);
            _renderer.setParam("aoDistance", _aoSamples);
            _renderer.setParam("maxPathLength", _maxDepth);
            _renderer.setParam("pixelFilter", (int)_pixelFilterType);
            _renderer.commit();
        }
        _lastSettingsVersion = currentSettingsVersion;
        ResetImage();
    }
    // XXX: Add clip planes support.

    // add lights
    GfVec3f up_light(up[0], up[1], up[2]);
    GfVec3f dir_light(dir[0], dir[1], dir[2]);
    if (_staticDirectionalLights) {
        up_light = { 0.f, 1.f, 0.f };
        dir_light = { -.1f, -.1f, -.8f };
    }
    GfVec3f right_light = GfCross(dir, up);
    std::vector<opp::Light> lights;

    // push scene lights
    for (auto sceneLight : _renderParam->GetLights()) {
        lights.push_back(sceneLight);
    }

    if (_ambientLight || light.size() == 0) {
        auto ambient = opp::Light("ambient");
        ambient.setParam("color", vec3f(1.f, 1.f, 1.f));
        ambient.setParam("intensity", 1.f);
        ambient.commit();
        lights.push_back(ambient);
    }

    if (_eyeLight) {
        auto eyeLight = opp::Light("distant");
        eyeLight.setParam("color", vec3f(1.f, 232.f / 255.f, 166.f / 255.f));
        eyeLight.setParam("direction", vec3f(dir_light[0], dir_light[1], dir_light[2]));
        eyeLight.setParam("intensity", 3.3f);
        eyeLight.setParam("visible", false);
        eyeLight.commit();
        lights.push_back(eyeLight);
    }
    const float angularDiameter = 4.5f;
    const float glToPTLightIntensityMultiplier = 1.5f;
    if (_keyLight) {
        auto keyLight = opp::Light("distant");
        auto keyHorz = -1.0f / tan(rad(45.0f)) * right_light;
        auto keyVert = 1.0f / tan(rad(70.0f)) * up_light;
        auto lightDir = -(keyVert + keyHorz);
        keyLight.setParam("color", vec3f(.8f, .8f, .8f));
        keyLight.setParam("direction", vec3f(lightDir[0], lightDir[1], lightDir[2]));
        keyLight.setParam("intensity", glToPTLightIntensityMultiplier);
        keyLight.setParam("angularDiameter", angularDiameter);
        keyLight.setParam("visible", false);
        keyLight.commit();
        lights.push_back(keyLight);
    }
    if (_fillLight) {
        auto fillLight = opp::Light("distant");
        auto fillHorz = 1.0f / tan(rad(30.0f)) * right_light;
        auto fillVert = 1.0f / tan(rad(45.0f)) * up_light;
        auto lightDir = (fillVert + fillHorz);
        fillLight.setParam("color", vec3f(.6f, .6f, .6f));
        fillLight.setParam("direction", vec3f(lightDir[0], lightDir[1], lightDir[2]));
        fillLight.setParam("intensity", glToPTLightIntensityMultiplier);
        fillLight.setParam("angularDiameter", angularDiameter);
        fillLight.setParam("visible", false);
        fillLight.commit();
        lights.push_back(fillLight);
    }
    if (_backLight) {
        auto backLight = opp::Light("distant");
        auto backHorz = 1.0f / tan(rad(60.0f)) * right_light;
        auto backVert = -1.0f / tan(rad(60.0f)) * up_light;
        auto lightDir = (backHorz + backVert);
        backLight.setParam("color", vec3f(.6f, .6f, .6f));
        backLight.setParam("direction", vec3f(lightDir[0], lightDir[1], lightDir[2]));
        backLight.setParam("intensity", glToPTLightIntensityMultiplier);
        backLight.setParam("angularDiameter", angularDiameter);
        backLight.setParam("visible", false);
        backLight.commit();
        lights.push_back(backLight);
    }
    OSPData lightArray
           = ospNewSharedData1D(lights.data(), OSP_LIGHT, lights.size());
    ospCommit(lightArray);
    if (_world) {
        _world.setParam("light", lightArray);
        _world.commit();
    }

    int currentSceneVersion = _sceneVersion->load();
    if (_lastRenderedVersion != currentSceneVersion) {
        ResetImage();
        _lastRenderedVersion = currentSceneVersion;
    }

    int currentModelVersion = _renderParam->GetModelVersion();
    if (_lastRenderedModelVersion != currentModelVersion) {
        ResetImage();
        _lastRenderedModelVersion = currentModelVersion;
        _pendingModelUpdate = true;
    }

    if (_pendingModelUpdate) {
        // release resources from last committed scene
        _oldInstances.resize(0);
        _world = opp::World();
        _world.commit();

        // create new model and populate with mesh instances
        _oldInstances.reserve(_renderParam->GetInstances().size());
        for (auto instance : _renderParam->GetInstances()) {
            _oldInstances.push_back(instance);
        }
        if (!_oldInstances.empty()) {
            auto data = ospNewSharedData1D(_oldInstances.data(), OSP_INSTANCE,
                                           _oldInstances.size());
            ospCommit(data);
            _world.setParam("instance", data);
        } else {
            _world.removeParam("instance");
        }
        _world.setParam("light", lightArray);
        _world.commit();
        _renderer.commit();
        _pendingModelUpdate = false;
        _renderParam->ClearInstances();
    }

    // Reset the sample buffer if it's been requested.
    if (_pendingResetImage) {
        _frameBuffer.resetAccumulation();
        _pendingResetImage = false;
        _numSamplesAccumulated = 0;
        if (_useDenoiser) {
            _spp = HdOSPRayConfig::GetInstance().samplesPerFrame;
            _renderer.setParam("pixelSamples", _spp);
            _renderer.commit();
        }
    }

    // Render the frame
    _frameBuffer.renderFrame(_renderer, _camera, _world);
    _numSamplesAccumulated += std::max(1, _spp);

    // Resolve the image buffer: find the average color per pixel by
    // dividing the summed color by the number of samples;
    // and convert the image into a GL-compatible format.
    void* rgba = _frameBuffer.map(OSP_FB_COLOR);
    memcpy((void*)&_colorBuffer[0], rgba, _width * _height * 4 * sizeof(float));
    _frameBuffer.unmap(rgba);
    if (_useDenoiser && _numSamplesAccumulated >= _denoiserSPPThreshold) {
        int newSPP
               = std::max((int)HdOSPRayConfig::GetInstance().samplesPerFrame, 1)
               * 4;
        if (_spp != newSPP) {
            _renderer.setParam("pixelSamples", _spp);
            _renderer.commit();
        }
    }

    // Blit!
    glDrawPixels(_width, _height, GL_RGBA, GL_FLOAT, _colorBuffer.data());
}

PXR_NAMESPACE_CLOSE_SCOPE
