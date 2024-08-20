// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "renderPass.h"
#include "renderParam.h"

#include "camera.h"
#include "config.h"
#include "context.h"
#include "lights/domeLight.h"
#include "lights/light.h"
#include "mesh.h"
#include "renderDelegate.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/stopwatch.h>

#include <ospray/ospray_util.h>

#include <iostream>

using namespace rkcommon::math;

PXR_NAMESPACE_USING_DIRECTIVE

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
    , _renderer(std::move(renderer))
    , _lastRenderedModelVersion(0)
    , _lastRenderedLightVersion(0)
    , _width(0)
    , _height(0)
    , _inverseViewMatrix(1.0f) // == identity
    , _inverseProjMatrix(1.0f) // == identity
    , _clearColor(0.0f, 0.0f, 0.0f, 0.f)
    , _renderParam(std::move(renderParam))
    , _colorBuffer(SdfPath::EmptyPath())
    , _depthBuffer(SdfPath::EmptyPath())
    , _cameraDepthBuffer(SdfPath::EmptyPath())
    , _normalBuffer(SdfPath::EmptyPath())
    , _primIdBuffer(SdfPath::EmptyPath())
    , _elementIdBuffer(SdfPath::EmptyPath())
    , _instIdBuffer(SdfPath::EmptyPath())
{
    _world = opp::World();
    _world.setParam("dynamicScene", true);
    _camera = opp::Camera("perspective");
    _cameraProjection = HdCamera::Projection::Perspective;
    _renderer.setParam("backgroundColor",
                       vec4f(_clearColor[0], _clearColor[1], _clearColor[2],
                             _clearColor[3]));
#if HDOSPRAY_ENABLE_DENOISER
    _denoiserLoaded = (ospLoadModule("denoiser") == OSP_NO_ERROR);
    if (!_denoiserLoaded)
        TF_WARN("osp: WARNING: could not load denoiser module");
#endif
    _renderer.setParam("maxPathLength", _maxDepth);
    _renderer.setParam("roulettePathLength", _russianRouletteStartDepth);
    _renderer.setParam("aoRadius", _aoRadius);
    _renderer.setParam("aoIntensity", _aoIntensity);
    _renderer.setParam("minContribution", _minContribution);
    _renderer.setParam("maxContribution", _maxContribution);
    _renderer.setParam("epsilon", 0.001f);
    _renderer.setParam("geometryLights", false);
    _rendererDirty = true;

    _lightsGroup.commit();
    _lightsInstance = opp::Instance(_lightsGroup);
    _lightsInstance.commit();
}

HdOSPRayRenderPass::~HdOSPRayRenderPass()
{
}

void
HdOSPRayRenderPass::ResetImage()
{
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
    // redraw on next execute
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
    TF_DEBUG_MSG(OSP, "ospRP::Execute\n");
    HdRenderDelegate* renderDelegate = GetRenderIndex()->GetRenderDelegate();

    // changes to renderer settings
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    _pendingSettingsUpdate = (_lastSettingsVersion != currentSettingsVersion);

    if (_pendingSettingsUpdate) {
        _ProcessSettings();
        _lastSettingsVersion = currentSettingsVersion;
    }

    if (!HdOSPRayConfig::GetInstance().usePathTracing) {
        _interactiveFrameBufferScale = 1.0f;
        _newInteractiveFrameBufferScale = 1.0f;
    }

    float updateInteractiveFrameBufferScale = _interactiveFrameBufferScale;
    _frameBufferDirty = false;
    _interactiveFrameBufferDirty = false;
    _interacting = false;

    HdRenderPassAovBindingVector aovBindings
           = renderPassState->GetAovBindings();

    // get clear color
    for (int aovIndex = 0; aovIndex < _aovBindings.size(); aovIndex++) {
        if (_aovNames[aovIndex].name == HdAovTokens->color) {
            GfVec4f clearColor
                   = _ComputeClearColor(_aovBindings[aovIndex].clearValue);
            if (clearColor != _clearColor) {
                _clearColor = clearColor;
                _renderer.setParam("backgroundColor",
                                   vec4f(_clearColor[0], _clearColor[1],
                                         _clearColor[2], _clearColor[3]));
                _rendererDirty = true;
                _pendingResetImage = true;
            }
        }
    }

    // check for aov updates
    bool aovDirty = (_aovBindings != aovBindings || _aovBindings.empty());
    if (aovDirty) {
        _hasColor = _hasDepth = _hasCameraDepth = _hasNormal = _hasPrimId
               = _hasInstId = false;
        // determine which aovs we need to fill
        for (int aovIndex = 0; aovIndex < aovBindings.size(); aovIndex++) {
            auto name = HdParsedAovToken(aovBindings[aovIndex].aovName).name;
            if (name == HdAovTokens->color) {
                _hasColor = true;
            } else if (name == HdAovTokens->depth) {
                _hasDepth = true;
            } else if (name == HdAovTokens->cameraDepth) {
                _hasCameraDepth = true;
            } else if (name == HdAovTokens->normal) {
                _hasNormal = true;
            } else if (name == HdAovTokens->primId) {
                _hasPrimId = true;
            } else if (name == HdAovTokens->elementId) {
                _hasElementId = true;
            } else if (name == HdAovTokens->instanceId) {
                _hasInstId = true;
            }
            _frameBufferDirty = true;
        }

        // update aov buffer maps.  if no aov buffers are specified, create a
        // color aov
        HdRenderPassAovBindingVector aovBindings
               = renderPassState->GetAovBindings();
        if (_aovBindings != aovBindings || _aovBindings.empty()) {
            if (aovBindings.size() == 0) {
                TF_DEBUG_MSG(OSP, "creating default color aov\n");
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
    bool useDenoiser = _denoiserLoaded && _useDenoiser
           && ((_numSamplesAccumulated + _spp) >= _denoiserSPPThreshold);
    _denoiserDirty = (useDenoiser != _denoiserState);
    auto inverseViewMatrix
           = renderPassState->GetWorldToViewMatrix().GetInverse();
    auto inverseProjMatrix
           = renderPassState->GetProjectionMatrix().GetInverse();
    bool cameraDirty = (inverseViewMatrix != _inverseViewMatrix
                        || inverseProjMatrix != _inverseProjMatrix);

    // dirty scene mesh representation
    int currentModelVersion = _renderParam->GetModelVersion();
    if (_lastRenderedModelVersion != currentModelVersion) {
        _pendingModelUpdate = true;
        _lastRenderedModelVersion = currentModelVersion;
        cameraDirty = true;
    }

    int currentMaterialVersion = _renderParam->GetMaterialVersion();
    if (_lastRenderedMaterialVersion != currentMaterialVersion) {
        _lastRenderedMaterialVersion = currentMaterialVersion;
        cameraDirty = true;
    }

    // dirty lights
    int currentLightVersion = _renderParam->GetLightVersion();
    if (_lastRenderedLightVersion != currentLightVersion) {
        _pendingLightUpdate = true;
        _lastRenderedLightVersion = currentLightVersion;
        cameraDirty = true;
    }

    // if we need to recommit the world
    bool worldDirty = _pendingModelUpdate;
    bool lightsDirty = _pendingLightUpdate;

    _pendingResetImage |= (_pendingModelUpdate || _pendingLightUpdate);
    _pendingResetImage |= (_frameBufferDirty || cameraDirty);

    const GfRect2i dataWindow = _GetDataWindow(renderPassState);

    if (_dataWindow != dataWindow) {
        _dataWindow = dataWindow;
        _frameBufferDirty = true;
        _width = dataWindow.GetWidth();
        _height = dataWindow.GetHeight();
    }

    // if progressively rendering an image and not in interactive mode, cancel
    // frame
    if ((_frameBufferDirty || _pendingResetImage || aovDirty)) {
        // cancel rendering
        if (_currentFrame.isValid()) {
            _currentFrame.osprayFrame.cancel();
            _currentFrame.osprayFrame.wait();
        }
        if (_interactiveEnabled)
            _interacting = true;

    } else if (_currentFrame.isValid()) { // nothing dirty, progressively
                                          // rendering next frame.
        // return until frame is ready
        if (!_currentFrame.osprayFrame.isReady())
            return;
        // progressively refined image is ready for display
        _currentFrame.osprayFrame.wait();

        _CopyFrameBuffer(renderPassState);

        _DisplayRenderBuffer(_currentFrame);
        _numSamplesAccumulated += std::max(1, _spp);

        // estimating scaling factor for interactive rendering based on
        // the current FPS and a given targetFPS
        float frameDuration = _currentFrame.Duration();
        float currentFPS = 1.0f / frameDuration;
        float scaleChange = std::sqrt(_interactiveTargetFPS / currentFPS);
        updateInteractiveFrameBufferScale
               = std::max(1.0f, _currentFrameBufferScale * scaleChange);
        updateInteractiveFrameBufferScale
               = std::min(updateInteractiveFrameBufferScale, 5.0f);
        updateInteractiveFrameBufferScale
               = 0.125f * std::ceil(updateInteractiveFrameBufferScale / 0.125f);
        if (_interactiveEnabled
            && fabs(updateInteractiveFrameBufferScale
                    - _currentFrameBufferScale)
                   > 0.3f) {
            _interactiveFrameBufferDirty = true;
            _newInteractiveFrameBufferScale = updateInteractiveFrameBufferScale;
        }
    }

    // setup for rendering the frame
    _UpdateFrameBuffer(useDenoiser, renderPassState);

    // setup renderer params
    if (!_interacting && _interactiveEnabled && !cameraDirty) {
        _renderer.setParam("maxPathLength", _maxDepth);
        _renderer.setParam("minContribution", _minContribution);
        _renderer.setParam("maxContribution", _maxContribution);
        _renderer.setParam("aoSamples", _aoSamples);
        _rendererDirty = true;
        _interactiveScaled = false;
    }

    // setup camera
    if (cameraDirty) {
        _inverseViewMatrix = inverseViewMatrix;
        _inverseProjMatrix = inverseProjMatrix;
        _ProcessCamera(renderPassState);
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

    // add mesh instances to world
    if (_pendingModelUpdate) {
        _ProcessInstances();
    }

    // add lights to world
    if (_pendingLightUpdate) {
        _ProcessLights();
        lightsDirty = true;
    }

    // world commit to prepare render
    if (_world && (worldDirty || lightsDirty)) {
        // Carson: world needs a lights only commit
        //   you cannot just update the lights group
        _world.commit();
    }

    // Reset the sample buffer if it's been requested.
    if (_pendingResetImage) {
        _frameBuffer.resetAccumulation();
        _pendingResetImage = false;
        _numSamplesAccumulated = 0;
    }

    // if interactive scaling is used, set interactive mode
    if (_interacting) {
        // framebuffer dirty or start interactive mode.
        // cancel rendered frame and display old frame if valid
        _renderer.setParam("minContribution", 0.1f);
        _renderer.setParam("maxContribution", 3.0f);
        _renderer.setParam("maxPathLength", min(4, _maxDepth));
        _renderer.setParam("aoSamples", (cameraDirty ? 0 : _aoSamples));
        _rendererDirty = true;
        _interactiveScaled = true;
    }

    if (_rendererDirty) {
        _renderer.commit();
        _rendererDirty = false;
    }

    opp::FrameBuffer frameBuffer = _frameBuffer;
    if (_interacting)
        frameBuffer = _interactiveFrameBuffer;

    // Async render the frame.
    if (!IsConverged()) {
        _currentFrame.osprayFrame
               = frameBuffer.renderFrame(_renderer, _camera, _world);
        if (_interacting) {
            _currentFrame.osprayFrame.wait();
            _CopyFrameBuffer(renderPassState);
            _DisplayRenderBuffer(_currentFrame);
            _currentFrame.osprayFrame = OSPFuture();
        }
    } else {
        for (int aovIndex = 0; aovIndex < _aovBindings.size(); aovIndex++) {
            auto ospRenderBuffer = dynamic_cast<HdOSPRayRenderBuffer*>(
                   _aovBindings[aovIndex].renderBuffer);
            ospRenderBuffer->SetConverged(true);
        }
    }
    TF_DEBUG_MSG(OSP, "ospRP::Execute done\n");
}

void
HdOSPRayRenderPass::_DisplayRenderBuffer(RenderFrame& renderBuffer)
{
    TF_DEBUG_MSG(OSP, "ospray render time: %f\n",
                 renderBuffer.osprayFrame.duration());
    static TfStopwatch timer;
    timer.Stop();
    double time = timer.GetSeconds();
    timer.Reset();
    timer.Start();
    TF_DEBUG_MSG(OSP, "display timer: %f\n", time);
    TF_DEBUG_MSG(OSP, "displayRB aov bindings %zu\n", _aovBindings.size());

    for (int aovIndex = 0; aovIndex < _aovBindings.size(); aovIndex++) {
        auto aovRenderBuffer = dynamic_cast<HdRenderBuffer*>(
               _aovBindings[aovIndex].renderBuffer);
        auto ospRenderBuffer = dynamic_cast<HdOSPRayRenderBuffer*>(
               _aovBindings[aovIndex].renderBuffer);
        if (!aovRenderBuffer || !ospRenderBuffer)
            continue;
        ospRenderBuffer->Map();
        if (_aovNames[aovIndex].name == HdAovTokens->color) {
            // ospray 2.12 + denoiser causing ghosting with 0 alphas, set alphas
            // to 1
            tbb::parallel_for(
                   tbb::blocked_range<int>(
                          0, renderBuffer.width * renderBuffer.height),
                   [&](tbb::blocked_range<int> r) {
                       for (int pIdx = r.begin(); pIdx < r.end(); ++pIdx) {
                           // denoiser in ospray 2.12 causes alpha to go to 0
                           if (_denoiserState)
                               renderBuffer.colorBuffer[pIdx].w = 1.f;
                       }
                   });
            _writeRenderBuffer<float>(ospRenderBuffer, renderBuffer,
                                      (float*)renderBuffer.colorBuffer.data(),
                                      4);
        } else if (_aovNames[aovIndex].name == HdAovTokens->depth) {
            _writeRenderBuffer<float>(ospRenderBuffer, renderBuffer,
                                      (float*)renderBuffer.depthBuffer.data(),
                                      1);
        } else if (_aovNames[aovIndex].name == HdAovTokens->cameraDepth) {
            _writeRenderBuffer<float>(
                   ospRenderBuffer, renderBuffer,
                   (float*)renderBuffer.cameraDepthBuffer.data(), 1);
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
        } else { // unsupported buffer, clear it
            if (ospRenderBuffer->GetFormat() == HdFormatInt32) {
                int32_t clearValue
                       = _aovBindings[aovIndex].clearValue.Get<int32_t>();
                ospRenderBuffer->Clear(1, &clearValue);
            } else if (ospRenderBuffer->GetFormat() == HdFormatFloat32) {
                float clearValue
                       = _aovBindings[aovIndex].clearValue.Get<float>();
                ospRenderBuffer->Clear(1, &clearValue);
            } else if (ospRenderBuffer->GetFormat() == HdFormatFloat32Vec3) {
                GfVec3f clearValue
                       = _aovBindings[aovIndex].clearValue.Get<GfVec3f>();
                ospRenderBuffer->Clear(2, clearValue.data());
            }
        }
        ospRenderBuffer->Unmap();
    }
    static float avgTime = 0.f;
    avgTime += time;
    static int avgCounter = 0;
    if (++avgCounter >= 15) {
        TF_DEBUG_MSG(OSP, "average fps: %f\n", avgTime / float(avgCounter));
        avgTime = 0.f;
        avgCounter = 0;
    }
    timer.Stop();
    time = timer.GetSeconds();
    timer.Start();
    TF_DEBUG_MSG(OSP, "display duration: %f\n", time);
}

void
HdOSPRayRenderPass::_ProcessCamera(
       HdRenderPassStateSharedPtr const& renderPassState)
{
    const HdCamera* camera = renderPassState->GetCamera();
    if (camera) {
        if (camera->GetProjection() != _cameraProjection) {
            _cameraProjection = camera->GetProjection();
            _camera = (_cameraProjection == HdCamera::Projection::Perspective)
                   ? opp::Camera("perspective")
                   : opp::Camera("orthographic");
        }
    }

    float aspect = _width / float(_height);
    _camera.setParam("aspect", aspect);
    TF_DEBUG_MSG(OSP, "aspect: %f\n", aspect);

    GfVec3f origin = GfVec3f(0, 0, 0);
    GfVec3f dir = GfVec3f(0, 0, -1);
    GfVec3f up = GfVec3f(0, 1, 0);
    dir = _inverseProjMatrix.Transform(dir);
    origin = _inverseViewMatrix.Transform(origin);
    dir = _inverseViewMatrix.TransformDir(dir).GetNormalized();
    up = _inverseViewMatrix.TransformDir(up).GetNormalized();

    _camera.setParam("position", vec3f(origin[0], origin[1], origin[2]));
    _camera.setParam("direction", vec3f(dir[0], dir[1], dir[2]));
    _camera.setParam("up", vec3f(up[0], up[1], up[2]));
    TF_DEBUG_MSG(OSP, "position: %f %f %f\n", origin[0], origin[1], origin[2]);
    TF_DEBUG_MSG(OSP, "direction: %f %f %f\n", dir[0], dir[1], dir[2]);
    TF_DEBUG_MSG(OSP, "up: %f %f %f\n", up[0], up[1], up[2]);

    if (_cameraProjection == HdCamera::Projection::Perspective) {
        double prjMatrix[4][4];
        renderPassState->GetProjectionMatrix().Get(prjMatrix);
        float fov = 2.0 * std::atan(1.0 / prjMatrix[1][1]) * 180.0 / M_PI;

        float focusDistance = 3.96f;
        float focalLength = 8.f;
        float fStop = 0.f;
        float aperture = 0.f;

        const HdOSPRayCamera* ospCamera
               = dynamic_cast<const HdOSPRayCamera*>(camera);
        if (ospCamera) {
            fStop = ospCamera->GetFStop();
            focusDistance = ospCamera->GetFocusDistance();
            focalLength = ospCamera->GetFocalLength();
        }

        if (fStop > 0.f)
            aperture = focalLength / fStop / 2.f * .1f;
        // only set if apterture over epsilon. Ran into issues on windows
        // setting DOF when 0.
        if (aperture > 1.0e-5) {
            _camera.setParam("focusDistance", focusDistance);
            _camera.setParam("apertureRadius", aperture);
        } else {
            _camera.setParam("apertureRadius", 0.f);
            _camera.setParam("focusDistance", 1.f);
        }
        _camera.setParam("fovy", fov);
        TF_DEBUG_MSG(OSP, "focusDistance: %f\n", focusDistance);
        TF_DEBUG_MSG(OSP, "apertureRadius: %f\n", aperture);
        TF_DEBUG_MSG(OSP, "fovy: %f\n", fov);
    }
    else { // orthographic
        float height = camera ? camera->GetVerticalAperture() : 100.0f;
        _camera.setParam("height", height);
        TF_DEBUG_MSG(OSP, "height: %f\n", height);
    }

    _camera.commit();
}

void
HdOSPRayRenderPass::_ProcessLights()
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
    for (const auto& l : _renderParam->GetHdOSPRayLights()) {
        if (l.second->IsVisible())
            lights.push_back(l.second->GetOSPLight());
    }

    if (_ambientLight || lights.empty()) {
        auto ambient = opp::Light("ambient");
        ambient.setParam("color", vec3f(1.f, 1.f, 1.f));
        ambient.setParam("intensity", 0.9f);
        ambient.setParam("visible", false);
        ambient.commit();
        lights.push_back(ambient);
    }
    _lightsGroup.setParam("light", opp::CopiedData(lights));
    _lightsGroup.commit();
    _pendingLightUpdate = false;
}

void
HdOSPRayRenderPass::_ProcessSettings()
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
           = (OSPPixelFilterType)renderDelegate->GetRenderSetting<int>(
                  HdOSPRayRenderSettingsTokens->pixelFilterType,
                  (int)OSPPixelFilterType::OSP_PIXELFILTER_GAUSS);
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
    float minContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->minContribution, _minContribution);
    float maxContribution = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->maxContribution, _maxContribution);
    bool useTonemapper = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->tmp_enabled,
           HdOSPRayConfig::GetInstance().tmp_enabled);
    GfVec4f shadowCatcherPlane = renderDelegate->GetRenderSetting<GfVec4f>(
        HdOSPRayRenderSettingsTokens->shadowCatcherPlane,
            _shadowCatcherPlane);
    bool geometryLights = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->geometryLights, _geometryLights);

    _interactiveTargetFPS = renderDelegate->GetRenderSetting<float>(
           HdOSPRayRenderSettingsTokens->interactiveTargetFPS,
           _interactiveTargetFPS);
    _interactiveEnabled = (_interactiveTargetFPS != 0);

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
        || russianRouletteStartDepth != _russianRouletteStartDepth
        || useTonemapper != _useTonemapper
        || shadowCatcherPlane != _shadowCatcherPlane
        || geometryLights != _geometryLights) {
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
        _useTonemapper = useTonemapper;
        _tonemapperDirty = true;
        _shadowCatcherPlane = shadowCatcherPlane;
        _geometryLights = geometryLights;
        _renderer.setParam("pixelSamples", _spp);
        _renderer.setParam("lightSamples", _lightSamples);
        _renderer.setParam("aoSamples", _aoSamples);
        _renderer.setParam("aoRadius", _aoRadius);
        _renderer.setParam("aoIntensity", _aoIntensity);
        _renderer.setParam("maxPathLength", _maxDepth);
        _renderer.setParam("roulettePathLength", _russianRouletteStartDepth);
        _renderer.setParam("minContribution", _minContribution);
        _renderer.setParam("maxContribution", _maxContribution);
        _renderer.setParam("roulettePathLength", _russianRouletteStartDepth);
        _renderer.setParam("pixelFilter", (int)_pixelFilterType);
        _renderer.setParam("shadowCatcherPlane", vec4f(_shadowCatcherPlane[0],
            _shadowCatcherPlane[1], _shadowCatcherPlane[2], _shadowCatcherPlane[3]));
        _renderer.setParam("geometryLights", _geometryLights);
        _rendererDirty = true;

        _pendingResetImage = true;
    }

    bool ambientLight = renderDelegate->GetRenderSetting<bool>(
           HdOSPRayRenderSettingsTokens->ambientLight, false);

    // checks if the lighting in the scene changed
    if (ambientLight != _ambientLight) {
        _ambientLight = ambientLight;
        _pendingLightUpdate = true;
        _pendingResetImage = true;
    }
}

void
HdOSPRayRenderPass::_ProcessInstances()
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
    _oldInstances.emplace_back(_lightsInstance);
    TF_DEBUG_MSG(OSP, "ospRP::num instances %zu\n", _oldInstances.size());
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

GfVec4f
HdOSPRayRenderPass::_ComputeClearColor(VtValue const& clearValue)
{
    HdTupleType type = HdGetValueTupleType(clearValue);
    if (type.count != 1) {
        return GfVec4f(0.0f, 0.0f, 0.0f, 1.0f);
    }

    switch (type.type) {
    case HdTypeFloatVec3: {
        GfVec3f f = *(static_cast<const GfVec3f*>(HdGetValueData(clearValue)));
        return GfVec4f(f[0], f[1], f[2], 1.0f);
    }
    case HdTypeFloatVec4: {
        GfVec4f f = *(static_cast<const GfVec4f*>(HdGetValueData(clearValue)));
        return f;
    }
    case HdTypeDoubleVec3: {
        GfVec3d f = *(static_cast<const GfVec3d*>(HdGetValueData(clearValue)));
        return GfVec4f(f[0], f[1], f[2], 1.0f);
    }
    case HdTypeDoubleVec4: {
        GfVec4d f = *(static_cast<const GfVec4d*>(HdGetValueData(clearValue)));
        return GfVec4f(f);
    }
    default:
        return GfVec4f(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void
HdOSPRayRenderPass::_CopyFrameBuffer(
       HdRenderPassStateSharedPtr const& renderPassState)
{
    opp::FrameBuffer frameBuffer = _frameBuffer;
    if (_interacting)
        frameBuffer = _interactiveFrameBuffer;

    // Resolve the image buffer: find the average color per pixel by
    // dividing the summed color by the number of samples;
    // and convert the image into a GL-compatible format.
    int frameSize = _currentFrame.width * _currentFrame.height;
    if (_hasColor) {
        vec4f* rgba = static_cast<vec4f*>(frameBuffer.map(OSP_FB_COLOR));
        if (rgba)
            std::copy(rgba, rgba + frameSize, _currentFrame.colorBuffer.data());
        frameBuffer.unmap(rgba);
    }
    if (_numSamplesAccumulated == 0 || _pendingResetImage) {
        if (_hasDepth || _hasCameraDepth) { // clip space depth
            float* depth = static_cast<float*>(frameBuffer.map(OSP_FB_DEPTH));
            if (depth) {
                if (_hasCameraDepth) {
                    std::copy(depth, depth + frameSize,
                              _currentFrame.cameraDepthBuffer.data());
                }
                if (_hasDepth) {
                    // convert depth to clip space
                    const auto viewMatrix
                           = renderPassState->GetWorldToViewMatrix();
                    const auto projMatrix
                           = renderPassState->GetProjectionMatrix();

                    const float w = _currentFrame.width;
                    const float h = _currentFrame.height;
                    tbb::parallel_for(0, (int)h, [&](int iy) {
                        tbb::parallel_for(0, (int)w, [&](int ix) {
                            const float x = ix;
                            const float y = iy;
                            const GfVec3f pos(2.f * (x / w) - 1.f,
                                              2.f * (y / h) - 1.f, -1.f);
                            GfVec3f dir = _inverseProjMatrix.Transform(pos);
                            GfVec3f origin = GfVec3f(0, 0, 0);
                            origin = _inverseViewMatrix.Transform(origin);
                            dir = _inverseViewMatrix.TransformDir(dir)
                                          .GetNormalized();
                            float& d = depth[static_cast<int>(y * w + x)];
                            GfVec3f hit = origin + dir * d;
                            hit = viewMatrix.Transform(hit);
                            hit = projMatrix.Transform(hit);
                            d = (hit[2] + 1.f) / 2.f;
                            if (isnan(d))
                                d = 1.f;
                        });
                    });

                    std::copy(depth, depth + frameSize,
                              _currentFrame.depthBuffer.data());
                }
            }
            frameBuffer.unmap(depth);
        }

        if (_hasNormal) {
            vec3f* normal = static_cast<vec3f*>(frameBuffer.map(OSP_FB_NORMAL));
            if (normal)
                std::copy(normal, normal + frameSize,
                          _currentFrame.normalBuffer.data());
            frameBuffer.unmap(normal);
        }

        if (_hasPrimId) {
            unsigned int* primId = static_cast<unsigned int*>(
                   frameBuffer.map(OSP_FB_ID_OBJECT));
            if (primId)
                std::copy(primId, primId + frameSize,
                          _currentFrame.primIdBuffer.data());
            frameBuffer.unmap(primId);
        }

        if (_hasElementId) {
            unsigned int* geomId = static_cast<unsigned int*>(
                   frameBuffer.map(OSP_FB_ID_PRIMITIVE));
            if (geomId)
                std::copy(geomId, geomId + frameSize,
                          _currentFrame.elementIdBuffer.data());
            frameBuffer.unmap(geomId);
        }

        if (_hasInstId) {
            unsigned int* instId = static_cast<unsigned int*>(
                   frameBuffer.map(OSP_FB_ID_INSTANCE));
            if (instId)
                std::copy(instId, instId + frameSize,
                          _currentFrame.instIdBuffer.data());
            frameBuffer.unmap(instId);
        }
    }
}

void
HdOSPRayRenderPass::_UpdateFrameBuffer(
       bool useDenoiser, HdRenderPassStateSharedPtr const& renderPassState)
{
    if (_frameBufferDirty) {
        _frameBuffer = opp::FrameBuffer(
               (int)_width, (int)_height, OSP_FB_RGBA32F,
               (_hasColor ? OSP_FB_COLOR : 0)
                      | (_hasDepth || _hasCameraDepth ? OSP_FB_DEPTH : 0)
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

        // if using a preframing version of USD, or if aovs are missing, create
        // buffers
#if PXR_VERSION > 2011
        if (!renderPassState->GetFraming().IsValid())
#endif
        {
            if (_hasColor)
                _colorBuffer.Allocate(GfVec3i(_width, _height, 1),
                                      HdFormatFloat32Vec4,
                                      /*multiSampled=*/false);
            if (_hasDepth)
                _depthBuffer.Allocate(GfVec3i(_width, _height, 1),
                                      HdFormatFloat32,
                                      /*multiSampled=*/false);
            if (_hasCameraDepth)
                _cameraDepthBuffer.Allocate(GfVec3i(_width, _height, 1),
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
        _interactiveFrameBufferDirty = true;
        _pendingResetImage = true;
        _aovDirty = true;
    }

    if (_interactiveFrameBufferDirty) {
        _interactiveFrameBufferScale = _newInteractiveFrameBufferScale;
        _interactiveFrameBuffer = opp::FrameBuffer(
               (int)(float(_width) / _interactiveFrameBufferScale),
               (int)(float(_height) / _interactiveFrameBufferScale),
               OSP_FB_RGBA32F,
               (_hasColor ? OSP_FB_COLOR : 0)
                      | (_hasDepth || _hasCameraDepth ? OSP_FB_DEPTH : 0)
                      | (_hasNormal ? OSP_FB_NORMAL : 0)
                      | (_hasElementId ? OSP_FB_ID_PRIMITIVE : 0)
                      | (_hasPrimId ? OSP_FB_ID_OBJECT : 0)
                      | (_hasInstId ? OSP_FB_ID_INSTANCE : 0));
        _interactiveFrameBuffer.commit();
        _interactiveFrameBufferDirty = false;
        if (_interacting)
            _pendingResetImage = true;
    }

    // setup image operations on frame
    if (_pendingResetImage || _denoiserDirty || _tonemapperDirty
        || _frameBufferDirty || _interactiveFrameBufferDirty) {
        std::vector<opp::ImageOperation> iops;
        if (useDenoiser && !_interacting) {
            opp::ImageOperation denoiser("denoiser");
            denoiser.commit();
            iops.emplace_back(denoiser);
        }
        if (_useTonemapper) {
            HdRenderDelegate* renderDelegate
                   = GetRenderIndex()->GetRenderDelegate();
            float exposure = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_exposure,
                   HdOSPRayConfig::GetInstance().tmp_exposure);
            float contrast = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_contrast,
                   HdOSPRayConfig::GetInstance().tmp_contrast);
            float shoulder = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_shoulder,
                   HdOSPRayConfig::GetInstance().tmp_shoulder);
            float midIn = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_midIn,
                   HdOSPRayConfig::GetInstance().tmp_midIn);
            float midOut = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_midOut,
                   HdOSPRayConfig::GetInstance().tmp_midOut);
            float hdrMax = renderDelegate->GetRenderSetting<float>(
                   HdOSPRayRenderSettingsTokens->tmp_hdrMax,
                   HdOSPRayConfig::GetInstance().tmp_hdrMax);
            bool acesColor = renderDelegate->GetRenderSetting<bool>(
                   HdOSPRayRenderSettingsTokens->tmp_acesColor,
                   HdOSPRayConfig::GetInstance().tmp_acesColor);
            opp::ImageOperation tonemapper("tonemapper");
            tonemapper.setParam("exposure", exposure);
            tonemapper.setParam("contrast", contrast);
            tonemapper.setParam("shoulder", shoulder);
            tonemapper.setParam("midIn", midIn);
            tonemapper.setParam("midOut", midOut);
            tonemapper.setParam("hdrMax", hdrMax);
            tonemapper.setParam("acesColor", acesColor);
            tonemapper.commit();
            iops.emplace_back(tonemapper);
        }
        opp::FrameBuffer frameBuffer = _frameBuffer;
        if (_interacting)
            frameBuffer = _interactiveFrameBuffer;
        if (!iops.empty()) {
            frameBuffer.setParam("imageOperation", opp::CopiedData(iops));
        } else
            frameBuffer.removeParam("imageOperation");

        _denoiserState = useDenoiser;
        frameBuffer.commit();
    }

    _frameBufferDirty = false;
    _interactiveFrameBufferDirty = false;
}