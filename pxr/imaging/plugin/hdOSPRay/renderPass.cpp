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

using namespace rkcommon::math;

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
    _renderer.setParam(
           "backgroundColor",
           vec4f(_clearColor[0], _clearColor[1], _clearColor[2], 1.f));

#if HDOSPRAY_ENABLE_DENOISER
    _denoiserLoaded = (ospLoadModule("denoiser") == OSP_NO_ERROR);
    if (!_denoiserLoaded)
        std::cout << "HDOSPRAY: WARNING: could not load denoiser module\n";
#endif
    _renderer.setParam("maxPathLength", 5);
    _renderer.setParam("roulettePathLength", 2);
    _renderer.setParam("aoRadius", 15.0f);
    _renderer.setParam("aoIntensity", _aoIntensity);
    _renderer.setParam("shadowsEnabled", true);
    _renderer.setParam("minContribution", _minContribution);
    _renderer.setParam("maxContribution", _maxContribution);
    _renderer.setParam("epsilon", 0.001f);
    _renderer.setParam("useGeometryLights", 0);

    _renderer.commit();

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // create OpenGL frame buffer texture
    glGenTextures(1, &framebufferTexture);
    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    return ((unsigned int)_numSamplesAccumulated
            >= (unsigned int)_samplesToConvergence);
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

    GfVec4f vp = renderPassState->GetViewport();
    bool frameBufferDirty = (_width != vp[2] || _height != vp[3]);
    bool useDenoiser
           = _denoiserLoaded && _useDenoiser && (_numSamplesAccumulated >= _denoiserSPPThreshold);
    bool denoiserDirty = (useDenoiser != _denoiserState);
    auto inverseViewMatrix
           = renderPassState->GetWorldToViewMatrix().GetInverse();
    auto inverseProjMatrix
           = renderPassState->GetProjectionMatrix().GetInverse();
    bool cameraDirty = (inverseViewMatrix != _inverseViewMatrix
                        || inverseProjMatrix != _inverseProjMatrix);
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    bool settingsDirty = (_lastSettingsVersion != currentSettingsVersion);
    int currentSceneVersion = _sceneVersion->load();
    if (_lastRenderedVersion != currentSceneVersion) {
        ResetImage();
    }
    int currentModelVersion = _renderParam->GetModelVersion();
    if (_lastRenderedModelVersion != currentModelVersion) {
        ResetImage();
        _pendingModelUpdate = true;
    }

    _pendingResetImage |= (frameBufferDirty || cameraDirty || settingsDirty);

    if (_currentFrame.isValid()) {
        if (frameBufferDirty || (_pendingResetImage && !_interacting)) {
            _currentFrame.osprayFrame.cancel();
            if (_previousFrame.isValid() && !frameBufferDirty)
                DisplayRenderBuffer(_previousFrame);
            _currentFrame.osprayFrame.wait();
            _interacting = true;
            _renderer.setParam("maxPathLength", 2);
            _renderer.commit();
        } else {
            if (!_currentFrame.osprayFrame.isReady()) {
                if (_previousFrame.isValid())
                    DisplayRenderBuffer(_previousFrame);
                return;
            }
            _currentFrame.osprayFrame.wait();

            opp::FrameBuffer frameBuffer = _frameBuffer;
            if (_interacting)
                frameBuffer = _interactiveFrameBuffer;

            // Resolve the image buffer: find the average color per pixel by
            // dividing the summed color by the number of samples;
            // and convert the image into a GL-compatible format.
            void* rgba = frameBuffer.map(OSP_FB_COLOR);
            memcpy(_currentFrame.colorBuffer.data(), rgba,
                   _currentFrame.width * _currentFrame.height * 4
                          * sizeof(float));
            frameBuffer.unmap(rgba);
            DisplayRenderBuffer(_currentFrame);
            _previousFrame = _currentFrame;
        }
    }

    if (_lastRenderedVersion != currentSceneVersion) {
        _lastRenderedVersion = currentSceneVersion;
    }
    if (_lastRenderedModelVersion != currentModelVersion) {
        _lastRenderedModelVersion = currentModelVersion;
        _pendingModelUpdate = true;
    }

    if (frameBufferDirty) {
        _width = vp[2];
        _height = vp[3];
        // reset OpenGL viewport and orthographic projection
        glViewport(0, 0, _width, _height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, _width, 0.0, _height, -1.0, 1.0);
        _frameBuffer
               = opp::FrameBuffer((int)_width, (int)_height, OSP_FB_RGBA32F,
                                  OSP_FB_COLOR | OSP_FB_ACCUM |
#if HDOSPRAY_ENABLE_DENOISER
                                         OSP_FB_NORMAL | OSP_FB_ALBEDO
                                         | OSP_FB_VARIANCE | OSP_FB_DEPTH |
#endif
                                         0);
        _frameBuffer.commit();
        _interactiveFrameBuffer
               = opp::FrameBuffer((int)_width / _interactiveFrameBufferScale,
                                  (int)_height / _interactiveFrameBufferScale,
                                  OSP_FB_RGBA32F, OSP_FB_COLOR);
        _interactiveFrameBuffer.commit();
        _currentFrame.colorBuffer.resize(_width * _height,
                                         vec4f({ 0.f, 0.f, 0.f, 0.f }));
        _pendingResetImage = true;
    }
    if (denoiserDirty) {
        if (useDenoiser) {
            _frameBuffer.setParam(
                   "imageOperation",
                   opp::CopiedData(opp::ImageOperation("denoiser")));

            // use more pixel samples when denoising.
            int newSPP
                   = std::max(
                            (int)HdOSPRayConfig::GetInstance().samplesPerFrame,
                            1)
                   * 4;
            if (_spp != newSPP) {
                _renderer.setParam("pixelSamples", _spp);
                _renderer.commit();
            }
        } else
            _frameBuffer.removeParam("imageOperation");

        _denoiserState = useDenoiser;
        _frameBuffer.commit();
    }

    if (_interacting != cameraDirty) {
        _renderer.setParam("maxPathLength", (cameraDirty ? 2 : _maxDepth));
        _renderer.commit();
    }

    if (cameraDirty) {
        ResetImage();
        _inverseViewMatrix = inverseViewMatrix;
        _inverseProjMatrix = inverseProjMatrix;
        _interacting = true;
    } else {
        _interacting = false;
    }

    GfVec3f origin = GfVec3f(0, 0, 0);
    GfVec3f dir = GfVec3f(0, 0, -1);
    GfVec3f up = GfVec3f(0, 1, 0);
    dir = _inverseProjMatrix.Transform(dir);
    origin = _inverseViewMatrix.Transform(origin);
    dir = _inverseViewMatrix.TransformDir(dir).GetNormalized();
    up = _inverseViewMatrix.TransformDir(up).GetNormalized();

    // set render frames size based on interaction mode
    if (_interacting) {
        if (_currentFrame.width != _width / _interactiveFrameBufferScale) {
            _currentFrame.width = _width / _interactiveFrameBufferScale;
            _currentFrame.height = _height / _interactiveFrameBufferScale;
            _currentFrame.colorBuffer.resize(_currentFrame.width
                                                    * _currentFrame.height,
                                             vec4f({ 0.f, 0.f, 0.f, 0.f }));
        }
    } else {
        if (_currentFrame.width != _width) {
            _currentFrame.width = _width;
            _currentFrame.height = _height;
            _currentFrame.colorBuffer.resize(_currentFrame.width
                                                    * _currentFrame.height,
                                             vec4f({ 0.f, 0.f, 0.f, 0.f }));
        }
    }

    float aspect = _width / float(_height);
    _camera.setParam("aspect", aspect);

    double prjMatrix[4][4];
    renderPassState->GetProjectionMatrix().Get(prjMatrix);
    float fov = 2.0 * std::atan(1.0 / prjMatrix[1][1]) * 180.0 / M_PI;

    _camera.setParam("position", vec3f(origin[0], origin[1], origin[2]));
    _camera.setParam("direction", vec3f(dir[0], dir[1], dir[2]));
    _camera.setParam("up", vec3f(up[0], up[1], up[2]));
    _camera.setParam("fovy", fov);
    _camera.commit();

    if (settingsDirty) {
        ProcessSettings();
    }
    // XXX: Add clip planes support.

    if (_pendingModelUpdate) {
        ProcessInstances();
    }

    // add lights
    ProcessLights();

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

    opp::FrameBuffer frameBuffer = _frameBuffer;
    if (_interacting)
        frameBuffer = _interactiveFrameBuffer;

    if (!IsConverged()) {
        // Render the frame
        _currentFrame.osprayFrame
               = frameBuffer.renderFrame(_renderer, _camera, _world);
        _numSamplesAccumulated += std::max(1, _spp);
    }
}

void
HdOSPRayRenderPass::DisplayRenderBuffer(RenderFrame& renderBuffer)
{

    // set initial OpenGL state
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderBuffer.width,
                 renderBuffer.height, 0, GL_RGBA, GL_FLOAT,
                 renderBuffer.colorBuffer.data());

    // clear current OpenGL color buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // render textured quad with OSPRay frame buffer contents
    glBegin(GL_QUADS);

    glTexCoord2f(0.f, 0.f);
    glVertex2f(0.f, 0.f);

    glTexCoord2f(0.f, 1.f);
    glVertex2f(0.f, _height);

    glTexCoord2f(1.f, 1.f);
    glVertex2f(_width, _height);

    glTexCoord2f(1.f, 0.f);
    glVertex2f(_width, 0.f);

    glEnd();
}

void
HdOSPRayRenderPass::ProcessLights()
{
    GfVec3f origin = GfVec3f(0, 0, 0);
    GfVec3f dir = GfVec3f(0, 0, -1);
    GfVec3f up = GfVec3f(0, 1, 0);
    dir = _inverseProjMatrix.Transform(dir);
    origin = _inverseViewMatrix.Transform(origin);
    dir = _inverseViewMatrix.TransformDir(dir).GetNormalized();
    up = _inverseViewMatrix.TransformDir(up).GetNormalized();
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

    if (_ambientLight || lights.empty()) {
        auto ambient = opp::Light("ambient");
        ambient.setParam("color", vec3f(1.f, 1.f, 1.f));
        ambient.setParam("intensity", 1.f);
        ambient.commit();
        lights.push_back(ambient);
    }

    if (_eyeLight) {
        auto eyeLight = opp::Light("distant");
        eyeLight.setParam("color", vec3f(1.f, 232.f / 255.f, 166.f / 255.f));
        eyeLight.setParam("direction",
                          vec3f(dir_light[0], dir_light[1], dir_light[2]));
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
        keyLight.setParam("direction",
                          vec3f(lightDir[0], lightDir[1], lightDir[2]));
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
        fillLight.setParam("direction",
                           vec3f(lightDir[0], lightDir[1], lightDir[2]));
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
        backLight.setParam("direction",
                           vec3f(lightDir[0], lightDir[1], lightDir[2]));
        backLight.setParam("intensity", glToPTLightIntensityMultiplier);
        backLight.setParam("angularDiameter", angularDiameter);
        backLight.setParam("visible", false);
        backLight.commit();
        lights.push_back(backLight);
    }
    if (_world) {
        _world.setParam("light", opp::CopiedData(lights));
        _world.commit();
    }
}

void
HdOSPRayRenderPass::ProcessSettings()
{
    HdRenderDelegate* renderDelegate = GetRenderIndex()->GetRenderDelegate();
    _samplesToConvergence = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->samplesToConvergence, 100);
    float aoDistance = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->aoDistance, 10.f);
    float aoIntensity = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->aoIntensity, 1.f);
#if HDOSPRAY_ENABLE_DENOISER
    _useDenoiser = _denoiserLoaded && renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->useDenoiser, true);
#endif
    auto pixelFilterType
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
    int minContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->minContribution, 0.1f);
    int maxContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->maxContribution, 3.f);
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
    if (spp != _spp || aoSamples != _aoSamples || aoDistance != _aoDistance
        || aoIntensity != _aoIntensity || maxDepth != _maxDepth
        || lSamples != _lightSamples || minContribution != _minContribution
        || _maxContribution != maxContribution
        || pixelFilterType != _pixelFilterType) {
        _spp = spp;
        _aoSamples = aoSamples;
        _maxDepth = maxDepth;
        _lightSamples = lSamples;
        _minContribution = minContribution;
        _maxContribution = maxContribution;
        _aoIntensity = aoIntensity;
        _aoDistance = aoDistance;
        _pixelFilterType = pixelFilterType;
        _renderer.setParam("pixelSamples", _spp);
        _renderer.setParam("lightSamples", _lightSamples);
        _renderer.setParam("aoSamples", _aoSamples);
        _renderer.setParam("aoRadius", _aoDistance);
        _renderer.setParam("aoIntensity", _aoIntensity);
        _renderer.setParam("maxPathLength", _maxDepth);
        _renderer.setParam("minContribution", _minContribution);
        _renderer.setParam("maxContribution", _maxContribution);
        _renderer.setParam("pixelFilter", (int)_pixelFilterType);
        _renderer.commit();
    }
    _lastSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    ResetImage();
}

void
HdOSPRayRenderPass::ProcessInstances()
{
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
    _world.commit();
    _renderer.commit();
    _pendingModelUpdate = false;
    _renderParam->ClearInstances();
}

PXR_NAMESPACE_CLOSE_SCOPE
