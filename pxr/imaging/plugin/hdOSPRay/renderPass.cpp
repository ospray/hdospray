//
// Copyright 2021 Intel
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

#include "renderPass.h"
#include "renderParam.h"

#include "camera.h"
#include "config.h"
#include "context.h"
#include "lights/domeLight.h"
#include "lights/light.h"
#include "mesh.h"
#include "renderDelegate.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/stopwatch.h>

#include <ospray/ospray_util.h>

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
       opp::Renderer renderer, std::shared_ptr<HdOSPRayRenderParam> renderParam)
    : HdRenderPass(index, collection)
    , _pendingResetImage(false)
    , _pendingModelUpdate(true)
    , _renderer(renderer)
    , _lastRenderedModelVersion(0)
    , _lastRenderedLightVersion(0)
    , _width(0)
    , _height(0)
    , _inverseViewMatrix(1.0f) // == identity
    , _inverseProjMatrix(1.0f) // == identity
    , _clearColor(0.0f, 0.0f, 0.0f)
    , _renderParam(renderParam)
    , _colorBuffer(SdfPath::EmptyPath())
    , _depthBuffer(SdfPath::EmptyPath())
    , _normalBuffer(SdfPath::EmptyPath())
    , _primIdBuffer(SdfPath::EmptyPath())
    , _elementIdBuffer(SdfPath::EmptyPath())
    , _instIdBuffer(SdfPath::EmptyPath())
{
    _world = opp::World();
    _camera = opp::Camera("perspective");
    _renderer.setParam(
           "backgroundColor",
           vec4f(_clearColor[0], _clearColor[1], _clearColor[2], 0.f));
#if HDOSPRAY_ENABLE_DENOISER
    _denoiserLoaded = (ospLoadModule("denoiser") == OSP_NO_ERROR);
    if (!_denoiserLoaded)
        std::cout << "HDOSPRAY: WARNING: could not load denoiser module\n";
#endif
    _renderer.setParam("maxPathLength", _maxDepth);
    _renderer.setParam("roulettePathLength", _russianRouletteStartDepth);
    _renderer.setParam("aoRadius", _aoRadius);
    _renderer.setParam("aoIntensity", _aoIntensity);
    _renderer.setParam("minContribution", _minContribution);
    _renderer.setParam("maxContribution", _maxContribution);
    _renderer.setParam("epsilon", 0.001f);
    _renderer.setParam("geometryLights", true);
    _renderer.commit();
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

static GfRect2i
_GetDataWindow(HdRenderPassStateSharedPtr const& renderPassState)
{
#if PXR_VERSION > 2011
    const CameraUtilFraming& framing = renderPassState->GetFraming();
    if (framing.IsValid()) {
        return framing.dataWindow;
    } else {
        // For applications that use the old viewport API instead of
        // the new camera framing API.
        const GfVec4f vp = renderPassState->GetViewport();
        return GfRect2i(GfVec2i(0), int(vp[2]), int(vp[3]));
    }
#else
    const GfVec4f vp = renderPassState->GetViewport();
    return GfRect2i(GfVec2i(0), int(vp[2]), int(vp[3]));
#endif
}

void
HdOSPRayRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                             TfTokenVector const& renderTags)
{
    TF_DEBUG_MSG(OSP_RP, "ospRP::Execute\n");
    HdRenderDelegate* renderDelegate = GetRenderIndex()->GetRenderDelegate();

    if (!HdOSPRayConfig::GetInstance().usePathTracing) {
        _interactiveFrameBufferScale = 1.0f;
    }

    float updateInteractiveFrameBufferScale = _interactiveFrameBufferScale;
    bool interactiveFramebufferDirty = false;

    HdRenderPassAovBindingVector aovBindings
           = renderPassState->GetAovBindings();
    bool aovDirty = (_aovBindings != aovBindings || _aovBindings.empty());
    if (aovDirty) {
        _hasColor = _hasDepth = _hasNormal = _hasPrimId = _hasInstId = false;
        for (int aovIndex = 0; aovIndex < aovBindings.size(); aovIndex++) {
            auto name = HdParsedAovToken(aovBindings[aovIndex].aovName).name;
            if (name == HdAovTokens->color) {
                _hasColor = true;
            } else if (name == HdAovTokens->depth) {
                _hasDepth = true;
            } else if (name == HdAovTokens->normal) {
                _hasNormal = true;
            } else if (name == HdAovTokens->primId) {
                _hasPrimId = true;
            } else if (name == HdAovTokens->elementId) {
                _hasElementId = true;
            } else if (name == HdAovTokens->instanceId) {
                _hasInstId = true;
            }
        }

        // Determine whether we need to update the renderer AOV bindings.
        //
        // It's possible for the passed in bindings to be empty, but that's
        // never a legal state for the renderer, so if that's the case we add
        // a color and depth aov.
        //
        // If the renderer AOV bindings are empty, force a bindings update so
        // that we always get a chance to add color/depth on the first time
        // through.
        HdRenderPassAovBindingVector aovBindings
               = renderPassState->GetAovBindings();
        if (_aovBindings != aovBindings || _aovBindings.empty()) {
            if (aovBindings.size() == 0) {
                TF_DEBUG_MSG(OSP_RP, "creating default color aov\n");
                HdRenderPassAovBinding colorAov;
                colorAov.aovName = HdAovTokens->color;
                colorAov.renderBuffer = &_colorBuffer;
                colorAov.clearValue
                       = VtValue(GfVec4f(0.0707f, 0.0707f, 0.0707f, 0.0f));
                aovBindings.push_back(colorAov);
                _hasColor = true;
            }
            SetAovBindings(aovBindings);
        }
        _pendingResetImage = true;
    }
    bool frameBufferDirty = false;
    bool useDenoiser = _denoiserLoaded && _useDenoiser
           && (_numSamplesAccumulated >= _denoiserSPPThreshold);
    bool denoiserDirty = (useDenoiser != _denoiserState);
    auto inverseViewMatrix
           = renderPassState->GetWorldToViewMatrix().GetInverse();
    auto inverseProjMatrix
           = renderPassState->GetProjectionMatrix().GetInverse();

    // if the camera has changed
    bool cameraDirty = (inverseViewMatrix != _inverseViewMatrix
                        || inverseProjMatrix != _inverseProjMatrix);

    // if we need to update the settings (rendering or lights)
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    _pendingSettingsUpdate = (_lastSettingsVersion != currentSettingsVersion);

    int currentModelVersion = _renderParam->GetModelVersion();
    if (_lastRenderedModelVersion != currentModelVersion) {
        _pendingModelUpdate = true;
        _lastRenderedModelVersion = currentModelVersion;
        cameraDirty = true;
    }

    int currentLightVersion = _renderParam->GetLightVersion();
    if (_lastRenderedLightVersion != currentLightVersion) {
        _pendingLightUpdate = true;
        _lastRenderedLightVersion = currentLightVersion;
        cameraDirty = true;
    }

    // if we need to recommit the world
    // bool worldDirty = _pendingModelUpdate || _pendingLightUpdate;
    bool worldDirty = _pendingModelUpdate;
    bool lightsDirty = _pendingLightUpdate;

    _pendingResetImage |= (_pendingModelUpdate || _pendingLightUpdate);
    _pendingResetImage |= (frameBufferDirty || cameraDirty);

    const GfRect2i dataWindow = _GetDataWindow(renderPassState);

    if (_dataWindow != dataWindow) {
        _dataWindow = dataWindow;
        frameBufferDirty = true;
        _width = dataWindow.GetWidth();
        _height = dataWindow.GetHeight();

#if PXR_VERSION > 2011
        if (!renderPassState->GetFraming().IsValid())
#endif
        {
            // Support clients that do not use the new framing API
            // and do not use AOVs.
            //
            // Note that we do not support the case of using the
            // new camera framing API without using AOVs.
            //
            if (_hasColor)
                _colorBuffer.Allocate(GfVec3i(_width, _height, 1),
                                      HdFormatFloat32Vec4,
                                      /*multiSampled=*/false);
            if (_hasDepth)
                _depthBuffer.Allocate(GfVec3i(_width, _height, 1),
                                      HdFormatFloat32,
                                      /*multiSampled=*/false);
            if (_hasNormal)
                _normalBuffer.Allocate(GfVec3i(_width, _height, 1),
                                       HdFormatFloat32Vec3,
                                       /*multiSampled=*/false);
            if (_hasPrimId)
                _primIdBuffer.Allocate(GfVec3i(_width, _height, 1),
                                       HdFormatInt32,
                                       /*multiSampled=*/false);
            if (_hasElementId)
                _elementIdBuffer.Allocate(GfVec3i(_width, _height, 1),
                                          HdFormatInt32,
                                          /*multiSampled=*/false);
            if (_hasInstId)
                _instIdBuffer.Allocate(GfVec3i(_width, _height, 1),
                                       HdFormatInt32,
                                       /*multiSampled=*/false);
        }
    }

    if (_currentFrame.isValid() && !aovDirty) {
        if (frameBufferDirty || (_pendingResetImage && !_interacting)) {
            // framebuffer dirty or start interactive mode.
            // cancel rendered frame and display old frame if valid
            _currentFrame.osprayFrame.cancel();
            if (_previousFrame.isValid() && !frameBufferDirty)
                DisplayRenderBuffer(_previousFrame);
            _currentFrame.osprayFrame.wait();
            _interacting = true;
            _renderer.setParam("maxPathLength", std::min(4, _maxDepth));
            _renderer.setParam("minContribution", 0.1f);
            _renderer.setParam("maxContribution", 3.0f);
            _renderer.setParam("aoSamples", 0);
            _renderer.commit();
        } else {
            // display previous ready frame until next one is ready
            // set next frame as interactive or final
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
            int frameSize = _currentFrame.width * _currentFrame.height;
            if (_hasColor) {
                vec4f* rgba
                       = static_cast<vec4f*>(frameBuffer.map(OSP_FB_COLOR));
                std::copy(rgba, rgba + frameSize,
                          _currentFrame.colorBuffer.data());
                frameBuffer.unmap(rgba);
            }

            if (_hasDepth) {
                float* depth
                       = static_cast<float*>(frameBuffer.map(OSP_FB_DEPTH));
                std::copy(depth, depth + frameSize,
                          _currentFrame.depthBuffer.data());
                frameBuffer.unmap(depth);
            }

            if (_hasNormal) {
                vec3f* normal
                       = static_cast<vec3f*>(frameBuffer.map(OSP_FB_NORMAL));
                std::copy(normal, normal + frameSize,
                          _currentFrame.normalBuffer.data());
                frameBuffer.unmap(normal);
            }

            if (_hasPrimId) {
                unsigned int* primId = static_cast<unsigned int*>(
                       frameBuffer.map(OSP_FB_ID_OBJECT));
                std::copy(primId, primId + frameSize,
                          _currentFrame.primIdBuffer.data());
                frameBuffer.unmap(primId);
            }

            if (_hasElementId) {
                unsigned int* geomId = static_cast<unsigned int*>(
                       frameBuffer.map(OSP_FB_ID_PRIMITIVE));
                std::copy(geomId, geomId + frameSize,
                          _currentFrame.elementIdBuffer.data());
                frameBuffer.unmap(geomId);
            }

            if (_hasInstId) {
                unsigned int* instId = static_cast<unsigned int*>(
                       frameBuffer.map(OSP_FB_ID_INSTANCE));
                std::copy(instId, instId + frameSize,
                          _currentFrame.instIdBuffer.data());
                frameBuffer.unmap(instId);
            }

            DisplayRenderBuffer(_currentFrame);
            _previousFrame = _currentFrame;

            // estimating scaling factor for interactive rendering based on
            // the current FPS and a given targetFPS
            float frameDuration = _currentFrame.Duration();
            float currentFPS = 1.0f / frameDuration;
            float scaleChange = std::sqrt(_interactiveTargetFPS / currentFPS);
            updateInteractiveFrameBufferScale
                   = std::max(1.0f, _currentFrameBufferScale * scaleChange);
            updateInteractiveFrameBufferScale
                   = std::min(updateInteractiveFrameBufferScale, 5.0f);
            updateInteractiveFrameBufferScale = 0.125f
                   * std::ceil(updateInteractiveFrameBufferScale / 0.125f);
            if (_interacting
                && updateInteractiveFrameBufferScale
                       != _currentFrameBufferScale) {
                interactiveFramebufferDirty = true;
                _interactiveFrameBufferScale
                       = updateInteractiveFrameBufferScale;
            }
        }
    }

    if (frameBufferDirty) {
        _frameBuffer = opp::FrameBuffer(
               (int)_width, (int)_height, OSP_FB_RGBA32F,
               (_hasColor ? OSP_FB_COLOR : 0) | (_hasDepth ? OSP_FB_DEPTH : 0)
                      | (_hasNormal ? OSP_FB_NORMAL : 0)
                      | (_hasElementId ? OSP_FB_ID_PRIMITIVE : 0)
                      | (_hasPrimId ? OSP_FB_ID_OBJECT : 0)
                      | (_hasInstId ? OSP_FB_ID_INSTANCE : 0) | OSP_FB_ACCUM |
#if HDOSPRAY_ENABLE_DENOISER
                      OSP_FB_ALBEDO | OSP_FB_VARIANCE | OSP_FB_NORMAL
                      | OSP_FB_DEPTH |
#endif
                      0);
        _frameBuffer.commit();
        _currentFrame.resize(_width * _height);
        interactiveFramebufferDirty = true;
        _pendingResetImage = true;
        _previousFrame = _currentFrame;
        aovDirty = true;
    }

    if (interactiveFramebufferDirty) {
        _interactiveFrameBuffer = opp::FrameBuffer(
               (int)(float(_width) / _interactiveFrameBufferScale),
               (int)(float(_height) / _interactiveFrameBufferScale),
               OSP_FB_RGBA32F,
               (_hasColor ? OSP_FB_COLOR : 0) | (_hasDepth ? OSP_FB_DEPTH : 0)
                      | (_hasNormal ? OSP_FB_NORMAL : 0)
                      | (_hasElementId ? OSP_FB_ID_PRIMITIVE : 0)
                      | (_hasPrimId ? OSP_FB_ID_OBJECT : 0)
                      | (_hasInstId ? OSP_FB_ID_INSTANCE : 0));
        _interactiveFrameBuffer.commit();
        interactiveFramebufferDirty = false;
        _pendingResetImage = true;
    }

    if (denoiserDirty) {
        if (useDenoiser) {
            _frameBuffer.setParam(
                   "imageOperation",
                   opp::CopiedData(opp::ImageOperation("denoiser")));
        } else
            _frameBuffer.removeParam("imageOperation");

        _denoiserState = useDenoiser;
        _frameBuffer.commit();
    }

    if (_interacting != cameraDirty) {
        _renderer.setParam("maxPathLength", (cameraDirty ? 4 : _maxDepth));
        _renderer.setParam("minContribution",
                           (cameraDirty ? 0.1f : _minContribution));
        _renderer.setParam("maxContribution",
                           (cameraDirty ? 3.0f : _maxContribution));
        _renderer.setParam("aoSamples", (cameraDirty ? 0 : _aoSamples));
        _renderer.commit();
    }

    if (cameraDirty) {
        _inverseViewMatrix = inverseViewMatrix;
        _inverseProjMatrix = inverseProjMatrix;
        _interacting = true;
        ProcessCamera(renderPassState);
    } else {
        _interacting = false;
    }

    // set render frames size based on interaction mode
    if (_interacting) {
        if (_currentFrame.width
                   != (unsigned int)(float(_width)
                                     / _interactiveFrameBufferScale)
            || _currentFrame.height
                   != (unsigned int)(float(_height)
                                     / _interactiveFrameBufferScale)) {
            _currentFrame.width
                   = (unsigned int)(float(_width)
                                    / _interactiveFrameBufferScale);
            _currentFrame.height
                   = (unsigned int)(float(_height)
                                    / _interactiveFrameBufferScale);
            _currentFrame.resize(_currentFrame.width * _currentFrame.height);
        }
        _currentFrameBufferScale = _interactiveFrameBufferScale;
        _pendingResetImage = true;
    } else {
        if (_currentFrame.width != _width || _currentFrame.height != _height) {
            _currentFrame.width = _width;
            _currentFrame.height = _height;
            _currentFrame.resize(_currentFrame.width * _currentFrame.height);
        }
        _currentFrameBufferScale = 1.0f;
    }

    if (_pendingSettingsUpdate) {
        ProcessSettings();
    }
    // XXX: Add clip planes support.

    if (_pendingModelUpdate) {
        ProcessInstances();
    }

    // add lights
    if (_pendingLightUpdate) {
        ProcessLights();
        lightsDirty = true;
    }

    if (_world && (worldDirty || lightsDirty)) {
        _world.commit();
    }

    // Reset the sample buffer if it's been requested.
    if (_pendingResetImage) {
        _frameBuffer.resetAccumulation();
        _pendingResetImage = false;
        _numSamplesAccumulated = 0;
    }

    opp::FrameBuffer frameBuffer = _frameBuffer;
    if (_interacting)
        frameBuffer = _interactiveFrameBuffer;

    if (!IsConverged()) {
        // Render the frame
        _currentFrame.osprayFrame
               = frameBuffer.renderFrame(_renderer, _camera, _world);
        if (!_interacting)
            _numSamplesAccumulated += std::max(1, _spp);
    } else {
        for (int aovIndex = 0; aovIndex < _aovBindings.size(); aovIndex++) {
            auto ospRenderBuffer = dynamic_cast<HdOSPRayRenderBuffer*>(
                   _aovBindings[aovIndex].renderBuffer);
            ospRenderBuffer->SetConverged(true);
        }
    }
    TF_DEBUG_MSG(OSP_RP, "ospRP::Execute done\n");
}

void
HdOSPRayRenderPass::DisplayRenderBuffer(RenderFrame& renderBuffer)
{
    TF_DEBUG_MSG(OSP_RP, "ospray render time: %f\n",
                 renderBuffer.osprayFrame.duration());
    static TfStopwatch timer;
    timer.Stop();
    double time = timer.GetSeconds();
    timer.Reset();
    timer.Start();
    TF_DEBUG_MSG(OSP_RP, "display timer: %f\n", time);

    TF_DEBUG_MSG(OSP_RP, "displayRB %zu\n", _aovBindings.size());

    for (int aovIndex = 0; aovIndex < _aovBindings.size(); aovIndex++) {
        auto aovRenderBuffer = dynamic_cast<HdRenderBuffer*>(
               _aovBindings[aovIndex].renderBuffer);
        auto ospRenderBuffer = dynamic_cast<HdOSPRayRenderBuffer*>(
               _aovBindings[aovIndex].renderBuffer);
        if (!aovRenderBuffer || !ospRenderBuffer)
            continue;
        if (_aovNames[aovIndex].name == HdAovTokens->color) {
            _writeRenderBuffer<float>(ospRenderBuffer, renderBuffer,
                                      (float*)renderBuffer.colorBuffer.data(),
                                      4);
        } else if (_aovNames[aovIndex].name == HdAovTokens->depth) {
            _writeRenderBuffer<float>(ospRenderBuffer, renderBuffer,
                                      (float*)renderBuffer.depthBuffer.data(),
                                      1);
        } else if (_aovNames[aovIndex].name == HdAovTokens->normal) {
            _writeRenderBuffer<float>(ospRenderBuffer, renderBuffer,
                                      (float*)renderBuffer.normalBuffer.data(),
                                      3);
        } else if (_aovNames[aovIndex].name == HdAovTokens->primId) {
            _writeRenderBuffer<int>(ospRenderBuffer, renderBuffer,
                                    (int*)renderBuffer.primIdBuffer.data(), 1);
        } else if (_aovNames[aovIndex].name == HdAovTokens->elementId) {
            _writeRenderBuffer<int>(ospRenderBuffer, renderBuffer,
                                    (int*)renderBuffer.elementIdBuffer.data(),
                                    1);
        } else if (_aovNames[aovIndex].name == HdAovTokens->instanceId) {
            _writeRenderBuffer<int>(ospRenderBuffer, renderBuffer,
                                    (int*)renderBuffer.instIdBuffer.data(), 1);
        }
    }
    timer.Stop();
    time = timer.GetSeconds();
    timer.Start();
    TF_DEBUG_MSG(OSP_RP, "display duration: %f\n", time);
}

void
HdOSPRayRenderPass::ProcessCamera(
       HdRenderPassStateSharedPtr const& renderPassState)
{
    GfVec3f origin = GfVec3f(0, 0, 0);
    GfVec3f dir = GfVec3f(0, 0, -1);
    GfVec3f up = GfVec3f(0, 1, 0);
    dir = _inverseProjMatrix.Transform(dir);
    origin = _inverseViewMatrix.Transform(origin);
    dir = _inverseViewMatrix.TransformDir(dir).GetNormalized();
    up = _inverseViewMatrix.TransformDir(up).GetNormalized();

    float aspect = _width / float(_height);
    _camera.setParam("aspect", aspect);

    double prjMatrix[4][4];
    renderPassState->GetProjectionMatrix().Get(prjMatrix);
    float fov = 2.0 * std::atan(1.0 / prjMatrix[1][1]) * 180.0 / M_PI;

    float focusDistance = 3.96f;
    float focalLength = 8.f;
    float fStop = 0.f;
    float aperture = 0.f;

    const HdCamera* camera = renderPassState->GetCamera();
    const HdOSPRayCamera* ospCamera
           = dynamic_cast<const HdOSPRayCamera*>(camera);
    if (ospCamera) {
        fStop = ospCamera->GetFStop();
        focusDistance = ospCamera->GetFocusDistance();
        focalLength = ospCamera->GetFocalLength();
    }

    if (fStop > 0.f)
        aperture = focalLength / fStop / 2.f * .1f;
    _camera.setParam("focusDistance", focusDistance);
    _camera.setParam("apertureRadius", aperture);
    _camera.setParam("position", vec3f(origin[0], origin[1], origin[2]));
    _camera.setParam("direction", vec3f(dir[0], dir[1], dir[2]));
    _camera.setParam("up", vec3f(up[0], up[1], up[2]));
    _camera.setParam("fovy", fov);
    _camera.commit();
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

    // if have image background, overide background color with image
    bool hasHDRI = false;
    // push scene lights
    for (auto l : _renderParam->GetHdOSPRayLights()) {
        if (l.second->IsVisible()) {
            lights.push_back(l.second->GetOSPLight());
            if (dynamic_cast<const HdOSPRayDomeLight*>(l.second)
                && l.second->IsVisibleToCamera()) {
                hasHDRI = true;
            }
        }
    }
    if (hasHDRI && HdOSPRayConfig::GetInstance().usePathTracing) {
        _renderer.setParam(
               "backgroundColor",
               vec4f(_clearColor[0], _clearColor[1], _clearColor[2], 1.f));
    }

    float glToPTLightIntensityMultiplier = 1.f;
    if (_eyeLight || _keyLight || _fillLight || _backLight)
        glToPTLightIntensityMultiplier /= 1.8f;

    if (_eyeLight) {
        auto eyeLight = opp::Light("distant");
        eyeLight.setParam("color", vec3f(1.f, 232.f / 255.f, 166.f / 255.f));
        eyeLight.setParam("direction",
                          vec3f(dir_light[0], dir_light[1], dir_light[2]));
        eyeLight.setParam("intensity", glToPTLightIntensityMultiplier);
        eyeLight.setParam("visible", false);
        eyeLight.commit();
        lights.push_back(eyeLight);
    }
    const float angularDiameter = 4.5f;
    if (_keyLight) {
        auto keyLight = opp::Light("distant");
        auto keyHorz = -1.0f / tan(rad(45.0f)) * right_light;
        auto keyVert = 1.0f / tan(rad(70.0f)) * up_light;
        auto lightDir = -(keyVert + keyHorz);
        keyLight.setParam("color", vec3f(.8f, .8f, .8f));
        keyLight.setParam("direction",
                          vec3f(lightDir[0], lightDir[1], lightDir[2]));
        keyLight.setParam("intensity", glToPTLightIntensityMultiplier * 1.3f);
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
    if (_ambientLight || lights.empty()) {
        auto ambient = opp::Light("ambient");
        ambient.setParam("color", vec3f(1.f, 1.f, 1.f));
        ambient.setParam("intensity", glToPTLightIntensityMultiplier);
        ambient.commit();
        lights.push_back(ambient);
    }
    if (_world) {
        _world.setParam("light", opp::CopiedData(lights));
    }
    _pendingLightUpdate = false;
}

void
HdOSPRayRenderPass::ProcessSettings()
{
    HdRenderDelegate* renderDelegate = GetRenderIndex()->GetRenderDelegate();
    int samplesToConvergence = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->samplesToConvergence,
           _samplesToConvergence);
    // TODO: check HdRenderSettingsTokens->convergedSamplesPerPixel
    float aoRadius = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->aoRadius, _aoRadius);
    float aoIntensity = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->aoIntensity, _aoIntensity);
#if HDOSPRAY_ENABLE_DENOISER
    _useDenoiser = _denoiserLoaded
           && renderDelegate->GetRenderSetting<bool>(
                  HdOSPRayRenderSettingsTokens->useDenoiser, true);
#endif
    auto pixelFilterType
           = (OSPPixelFilterTypes)renderDelegate->GetRenderSetting<int>(
                  HdOSPRayRenderSettingsTokens->pixelFilterType,
                  (int)OSPPixelFilterTypes::OSP_PIXELFILTER_GAUSS);
    int spp = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->samplesPerFrame, _spp);
    int lSamples = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->lightSamples, -1);
    int aoSamples = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->ambientOcclusionSamples, _aoSamples);
    int maxDepth = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->maxDepth, _maxDepth);
    int russianRouletteStartDepth = renderDelegate->GetRenderSetting<int>(
           HdOSPRayRenderSettingsTokens->russianRouletteStartDepth,
           _russianRouletteStartDepth);
    int minContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->minContribution, _minContribution);
    int maxContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->maxContribution, _maxContribution);

    _interactiveTargetFPS = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->interactiveTargetFPS,
           _interactiveTargetFPS);

    if (samplesToConvergence != _samplesToConvergence) {
        _samplesToConvergence = samplesToConvergence;
        _pendingResetImage = true;
    }

    // checks if the renderer settings changed
    if (spp != _spp || aoSamples != _aoSamples || aoRadius != _aoRadius
        || aoIntensity != _aoIntensity || maxDepth != _maxDepth
        || lSamples != _lightSamples || minContribution != _minContribution
        || _maxContribution != maxContribution
        || pixelFilterType != _pixelFilterType
        || russianRouletteStartDepth != _russianRouletteStartDepth) {
        _spp = spp;
        _aoSamples = aoSamples;
        _maxDepth = maxDepth;
        _russianRouletteStartDepth = russianRouletteStartDepth;
        _lightSamples = lSamples;
        _minContribution = minContribution;
        _maxContribution = maxContribution;
        _aoIntensity = aoIntensity;
        _aoRadius = aoRadius;
        _pixelFilterType = pixelFilterType;
        _renderer.setParam("pixelSamples", _spp);
        _renderer.setParam("lightSamples", _lightSamples);
        _renderer.setParam("aoSamples", _aoSamples);
        _renderer.setParam("aoRadius", _aoRadius);
        _renderer.setParam("aoIntensity", _aoIntensity);
        _renderer.setParam("maxPathLength", _maxDepth);
        _renderer.setParam("roulettePathLength", _russianRouletteStartDepth);
        _renderer.setParam("minContribution", _minContribution);
        _renderer.setParam("maxContribution", _maxContribution);
        _renderer.setParam("pixelFilter", (int)_pixelFilterType);
        _renderer.commit();

        _pendingResetImage = true;
    }

    bool ambientLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->ambientLight, false);
    // default static ospray directional lights
    bool staticDirectionalLights = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->staticDirectionalLights, false);
    // eye, key, fill, and back light are copied from USD GL (Storm)
    // defaults.
    bool eyeLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->eyeLight, false);
    bool keyLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->keyLight, false);
    bool fillLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->fillLight, false);
    bool backLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->backLight, false);

    // checks if the lighting in the scene changed
    if (ambientLight != _ambientLight
        || staticDirectionalLights != _staticDirectionalLights
        || eyeLight != _eyeLight || keyLight != _keyLight
        || fillLight != _fillLight || backLight != _backLight) {
        _ambientLight = ambientLight;
        _staticDirectionalLights = staticDirectionalLights;
        _eyeLight = eyeLight;
        _keyLight = keyLight;
        _fillLight = fillLight;
        _backLight = backLight;
        _pendingLightUpdate = true;
        _pendingResetImage = true;
    }
}

void
HdOSPRayRenderPass::ProcessInstances()
{
    // release resources from last committed scene
    _oldInstances.resize(0);
    // create new model and populate with mesh instances
    for (auto hdOSPRayMesh : _renderParam->GetHdOSPRayMeshes()) {
        hdOSPRayMesh->AddOSPInstances(_oldInstances);
    }
    for (auto hdOSPRayBasisCurves : _renderParam->GetHdOSPRayBasisCurves()) {
        hdOSPRayBasisCurves->AddOSPInstances(_oldInstances);
    }
    TF_DEBUG_MSG(OSP_RP, "ospRP::process instances %zu\n",
                 _oldInstances.size());
    if (!_oldInstances.empty()) {
        opp::CopiedData data = opp::CopiedData(
               _oldInstances.data(), OSP_INSTANCE, _oldInstances.size());
        data.commit();
        _world.setParam("instance", data);
    } else {
        _world.removeParam("instance");
    }
    _pendingModelUpdate = false;
}

void
HdOSPRayRenderPass::SetAovBindings(
       HdRenderPassAovBindingVector const& aovBindings)
{
    _aovBindings = aovBindings;
    _aovNames.resize(_aovBindings.size());
    for (size_t i = 0; i < _aovBindings.size(); ++i) {
        _aovNames[i] = HdParsedAovToken(_aovBindings[i].aovName);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
