// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "renderBuffer.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/debug.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/pxr.h>
#include "pxr/base/gf/rect2i.h"

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <pxr/base/work/loops.h>

#include "config.h"

namespace opp = ospray::cpp;

using namespace rkcommon::math;

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayRenderParam;

/// \class HdOSPRayRenderPass
class HdOSPRayRenderPass final : public HdRenderPass {
public:
    HdOSPRayRenderPass(HdRenderIndex* index,
                       HdRprimCollection const& collection,
                       opp::Renderer renderer,
                       std::shared_ptr<HdOSPRayRenderParam> renderParam);

    virtual ~HdOSPRayRenderPass();

    /// Mark the frame as dirty for next pass
    virtual void ResetImage();

    /// Converged based on samples per pixel and samples to convergence settings
    virtual bool IsConverged() const override;

    // manages ospray state and buffers of a frame
    struct RenderFrame {
        opp::Future osprayFrame;
        unsigned int width { 0 };
        unsigned int height { 0 };
        // The resolved output buffer, in GL_RGBA. This is an intermediate
        // between _sampleBuffer and the GL framebuffer.
        std::vector<vec4f> colorBuffer;
        std::vector<float> depthBuffer;
        std::vector<float> cameraDepthBuffer;
        std::vector<vec3f> normalBuffer;
        std::vector<unsigned int> primIdBuffer;
        std::vector<unsigned int> elementIdBuffer;
        std::vector<unsigned int> instIdBuffer;

        bool isValid()
        {
            return osprayFrame;
        }

        inline float Duration()
        {
            return osprayFrame.duration();
        }

        inline void resize(size_t size)
        {
            colorBuffer.resize(size, vec4f({ 0.f, 0.f, 0.f, 0.f }));
            depthBuffer.resize(size, FLT_MAX);
            cameraDepthBuffer.resize(size, FLT_MAX);
            normalBuffer.resize(size, vec3f({ 0.f, 1.f, 0.f }));
            primIdBuffer.resize(size, -1);
            elementIdBuffer.resize(size, -1);
            instIdBuffer.resize(size, -1);
        }
    };

    void SetAovBindings(HdRenderPassAovBindingVector const& aovBindings);

    HdRenderPassAovBindingVector const& GetAovBindings() const
    {
        return _aovBindings;
    }

protected:
    /// Draw and/or display the scene
    /// rendering is asyncronous, will need to be called repeatedly until
    /// isconverged
    virtual void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
                          TfTokenVector const& renderTags) override;

    // override interal dirty call
    virtual void _MarkCollectionDirty() override;

    virtual void
    _ProcessCamera(HdRenderPassStateSharedPtr const& renderPassState);
    virtual void _ProcessLights();
    virtual void _ProcessSettings();
    virtual void _ProcessInstances();
    virtual void
    _CopyFrameBuffer(HdRenderPassStateSharedPtr const& renderPassState);
    virtual void _DisplayRenderBuffer(RenderFrame& renderFrame);

private:
    /// @brief  helper function to write data into a renderbuffer
    /// @tparam T data type, eg vec3f
    /// @param ospRenderBuffer
    /// @param renderFrame
    /// @param data  source data
    /// @param numElements  number of type T elements to write
    template <class T>
    void _writeRenderBuffer(HdOSPRayRenderBuffer* ospRenderBuffer,
                            RenderFrame& renderFrame, T* data, int numElements)
    {
        int aovWidth = ospRenderBuffer->GetWidth();
        int aovHeight = ospRenderBuffer->GetHeight();
        if (aovWidth >= renderFrame.width && aovHeight >= renderFrame.height) {
            ospRenderBuffer->Map();
            float xscale = float(renderFrame.width) / float(aovWidth);
            float yscale = float(renderFrame.height) / float(aovHeight);
            tbb::parallel_for(
                   tbb::blocked_range<int>(0, aovWidth * aovHeight),
                   [&](tbb::blocked_range<int> r) {
                       for (int pIdx = r.begin(); pIdx < r.end(); ++pIdx) {
                           int j = pIdx / aovWidth;
                           int i = pIdx - j * aovWidth;
                           int js = j * xscale;
                           int is = i * yscale;
                           ospRenderBuffer->Write(
                                  GfVec3i(i, j, 1), numElements,
                                  &(data[(js * renderFrame.width + is)
                                         * numElements]));
                       }
                   });
            ospRenderBuffer->Unmap();
        } else
            TF_WARN("displayrenderbuffer size out of sync");
    };

    // Return the clear color to use for the given VtValue
    static GfVec4f _ComputeClearColor(VtValue const& clearValue);

    // self allocate aovs if renderpassstate has invalid framing
    void _UpdateFrameBuffer(bool useDenoiser,
                            HdRenderPassStateSharedPtr const& renderPassState);

    bool _pendingResetImage { true };
    bool _interactiveFrameBufferDirty { true };
    bool _frameBufferDirty { true };
    bool _denoiserDirty { true };
    bool _aovDirty { true };
    bool _pendingModelUpdate { true };
    bool _pendingLightUpdate { true };
    bool _pendingSettingsUpdate { true };

    opp::FrameBuffer _frameBuffer;
    opp::FrameBuffer _interactiveFrameBuffer;
    GfRect2i _dataWindow;
    HdRenderPassAovBindingVector _aovBindings;
    HdParsedAovTokenVector _aovNames;
    HdOSPRayRenderBuffer _colorBuffer;
    bool _hasColor { true };
    HdOSPRayRenderBuffer _depthBuffer;
    bool _hasDepth { false };
    HdOSPRayRenderBuffer _cameraDepthBuffer;
    bool _hasCameraDepth { false };
    HdOSPRayRenderBuffer _normalBuffer;
    bool _hasNormal { false };
    HdOSPRayRenderBuffer _primIdBuffer;
    bool _hasPrimId { false };
    HdOSPRayRenderBuffer _elementIdBuffer;
    bool _hasElementId { false };
    HdOSPRayRenderBuffer _instIdBuffer;
    bool _hasInstId { false };
    float _currentFrameBufferScale { 1.0f };
    float _interactiveFrameBufferScale { 2.0f };
    float _newInteractiveFrameBufferScale {
        2.0f
    }; // to be updated next new generation
    float _interactiveTargetFPS { HDOSPRAY_DEFAULT_INTERACTIVE_TARGET_FPS };

    opp::Renderer _renderer;
    bool _rendererDirty { true };

    bool _interacting { false };
    bool _interactiveScaled { false }; // is fb/param scaled from interaction?
    bool _interactiveEnabled { true}; // disabled by setting interactivetargetfps to 0

    int _lastRenderedModelVersion { -1 };
    int _lastRenderedLightVersion { -1 };
    int _lastRenderedMaterialVersion { -1 };
    int _lastSettingsVersion { -1 };

    RenderFrame _currentFrame;

    // viewport width
    unsigned int _width { 0 };
    // viewport height
    unsigned int _height { 0 };

    opp::Camera _camera;
    GfVec3f _cameraDir { 0.f, 0.f, -1.f };
    GfVec3f _cameraOrigin { 0.f, 0.f, 1.f };

    // camera space to world space
    GfMatrix4d _inverseViewMatrix;
    GfMatrix4d _inverseProjMatrix;

    GfVec4f _clearColor;

    std::shared_ptr<HdOSPRayRenderParam> _renderParam;

    std::vector<opp::Instance> _oldInstances; // instances added to last model
    opp::Group _lightsGroup;
    opp::Instance _lightsInstance;
    opp::World _world = nullptr; // the last model created

    int _numSamplesAccumulated { 0 }; // number of rendered frames not cleared
    int _spp { HDOSPRAY_DEFAULT_SPP };
    bool _useDenoiser { false };
    bool _useTonemapper { true };
    bool _tonemapperDirty { true };
    bool _denoiserLoaded { false }; // did the module successfully load?
    bool _denoiserState { false };
    OSPPixelFilterType _pixelFilterType {
        OSPPixelFilterType::OSP_PIXELFILTER_GAUSS
    };
    int _samplesToConvergence { HDOSPRAY_DEFAULT_SPP_TO_CONVERGE };
    int _denoiserSPPThreshold { 6 };
    int _aoSamples { HDOSPRAY_DEFAULT_AO_SAMPLES };
    int _lightSamples { -1 };
    bool _staticDirectionalLights { true };
    bool _ambientLight { true };

    int _maxDepth { HDOSPRAY_DEFAULT_MAX_DEPTH };
    int _russianRouletteStartDepth { HDOSPRAY_DEFAULT_RR_START_DEPTH };
    float _minContribution { HDOSPRAY_DEFAULT_MIN_CONTRIBUTION };
    float _maxContribution { HDOSPRAY_DEFAULT_MAX_CONTRIBUTION };
    GfVec4f _shadowCatcherPlane;
    bool _geometryLights {false};

    float _aoRadius { HDOSPRAY_DEFAULT_AO_RADIUS };
    float _aoIntensity { HDOSPRAY_DEFAULT_AO_INTENSITY };
};