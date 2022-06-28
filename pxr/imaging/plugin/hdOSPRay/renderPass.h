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
#ifndef HDOSPRAY_RENDER_PASS_H
#define HDOSPRAY_RENDER_PASS_H

#include "renderBuffer.h"

#include "pxr/base/gf/rect2i.h"
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/debug.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <pxr/base/work/loops.h>

#include "config.h"

namespace opp = ospray::cpp;

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE
TF_DEBUG_CODES(OSP_RP);

class HdOSPRayRenderParam;

/// \class HdOSPRayRenderPass
///
/// HdRenderPass represents a single render iteration, rendering a view of the
/// scene (the HdRprimCollection) for a specific viewer (the camera/viewport
/// parameters in HdRenderPassState) to the current draw target.
///
/// This class does so by raycasting into the OSPRay scene.
///
class HdOSPRayRenderPass final : public HdRenderPass {
public:
    /// Renderpass constructor.
    ///   \param index The render index containing scene data to render.
    ///   \param collection The initial rprim collection for this renderpass.
    ///   \param scene The OSPRay scene to raycast into.
    HdOSPRayRenderPass(HdRenderIndex* index,
                       HdRprimCollection const& collection,
                       opp::Renderer renderer,
                       std::shared_ptr<HdOSPRayRenderParam> renderParam);

    /// Renderpass destructor.
    virtual ~HdOSPRayRenderPass();

    // -----------------------------------------------------------------------
    // HdRenderPass API

    /// Clear the sample buffer (when scene or camera changes).
    virtual void ResetImage();

    /// Determine whether the sample buffer has enough samples.
    ///   \return True if the image has enough samples to be considered final.
    virtual bool IsConverged() const override;

    struct RenderFrame {
        opp::Future osprayFrame;
        unsigned int width { 0 };
        unsigned int height { 0 };
        // The resolved output buffer, in GL_RGBA. This is an intermediate
        // between _sampleBuffer and the GL framebuffer.
        std::vector<vec4f> colorBuffer;
        std::vector<float> depthBuffer;
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
            normalBuffer.resize(size, vec3f({ 0.f, 1.f, 0.f }));
            primIdBuffer.resize(size, -1);
            elementIdBuffer.resize(size, -1);
            instIdBuffer.resize(size, -1);
        }
    };

    virtual void DisplayRenderBuffer(RenderFrame& renderFrame);

    /// Set the aov bindings to use for rendering.
    ///   \param aovBindings A list of aov bindings.
    void SetAovBindings(HdRenderPassAovBindingVector const& aovBindings);

    /// Get the aov bindings being used for rendering.
    ///   \return the current aov bindings.
    HdRenderPassAovBindingVector const& GetAovBindings() const
    {
        return _aovBindings;
    }

protected:
    // -----------------------------------------------------------------------
    // HdRenderPass API

    /// Draw the scene with the bound renderpass state.
    ///   \param renderPassState Input parameters (including viewer parameters)
    ///                          for this renderpass.
    ///   \param renderTags Which rendertags should be drawn this pass.
    virtual void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
                          TfTokenVector const& renderTags) override;

    /// Update internal tracking to reflect a dirty collection.
    virtual void _MarkCollectionDirty() override;

    virtual void
    ProcessCamera(HdRenderPassStateSharedPtr const& renderPassState);
    virtual void ProcessLights();
    virtual void ProcessSettings();
    virtual void ProcessInstances();

private:
    // -----------------------------------------------------------------------
    // Internal API

    // Specify a new viewport size for the sample buffer. Note: the caller
    // should also call ResetImage().
    void _ResizeSampleBuffer(unsigned int width, unsigned int height);

    template <class T>
    void _writeRenderBuffer(HdOSPRayRenderBuffer* ospRenderBuffer,
                            RenderFrame& renderBuffer, T* data, int numElements)
    {
        int aovWidth = ospRenderBuffer->GetWidth();
        int aovHeight = ospRenderBuffer->GetHeight();
        if (aovWidth >= renderBuffer.width
            && aovHeight >= renderBuffer.height) {
            ospRenderBuffer->Map();
            float xscale = float(renderBuffer.width) / float(aovWidth);
            float yscale = float(renderBuffer.height) / float(aovHeight);
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
                                  &(data[(js * renderBuffer.width + is)
                                         * numElements]));
                       }
                   });
            ospRenderBuffer->Unmap();
        } else
            TF_CODING_ERROR("ERROR: displayrenderbuffer size out of sync\n");
    };

    // The sample buffer is cleared in Execute(), so this flag records whether
    // ResetImage() has been called since the last Execute().
    bool _pendingResetImage { true };
    bool _pendingModelUpdate { true };
    bool _pendingLightUpdate { true };
    bool _pendingSettingsUpdate { true };

    opp::FrameBuffer _frameBuffer;
    opp::FrameBuffer _interactiveFrameBuffer;
    // The pixels written to. Like viewport in OpenGL,
    // but coordinates are y-Down.
    GfRect2i _dataWindow;
    HdRenderPassAovBindingVector _aovBindings;
    HdParsedAovTokenVector _aovNames;
    HdOSPRayRenderBuffer _colorBuffer;
    bool _hasColor { true };
    HdOSPRayRenderBuffer _depthBuffer;
    bool _hasDepth { false };
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
    float _interactiveTargetFPS { HDOSPRAY_DEFAULT_INTERACTIVE_TARGET_FPS };

    opp::Renderer _renderer;

    bool _interacting { true };

    int _lastRenderedModelVersion { -1 };
    int _lastRenderedLightVersion { -1 };
    int _lastSettingsVersion { -1 };

    RenderFrame _currentFrame;
    RenderFrame _previousFrame;

    // The width of the viewport we're rendering into.
    unsigned int _width { 0 };
    // The height of the viewport we're rendering into.
    unsigned int _height { 0 };

    opp::Camera _camera;

    // The inverse view matrix: camera space to world space.
    GfMatrix4d _inverseViewMatrix;
    // The inverse projection matrix: NDC space to camera space.
    GfMatrix4d _inverseProjMatrix;

    // The color of a ray miss.
    GfVec3f _clearColor;

    std::shared_ptr<HdOSPRayRenderParam> _renderParam;

    std::vector<opp::Instance> _oldInstances; // instances added to last model
    opp::World _world = nullptr; // the last model created

    int _numSamplesAccumulated { 0 }; // number of rendered frames not cleared
    int _spp { HDOSPRAY_DEFAULT_SPP };
    bool _useDenoiser { false };
    bool _denoiserLoaded { false }; // did the module successfully load?
    bool _denoiserState { false };
    OSPPixelFilterTypes _pixelFilterType {
        OSPPixelFilterTypes::OSP_PIXELFILTER_GAUSS
    };
    int _samplesToConvergence { HDOSPRAY_DEFAULT_SPP_TO_CONVERGE };
    int _denoiserSPPThreshold { 6 };
    int _aoSamples { HDOSPRAY_DEFAULT_AO_SAMPLES };
    int _lightSamples { -1 };
    bool _staticDirectionalLights { true };
    bool _ambientLight { true };
    bool _eyeLight { true };
    bool _keyLight { true };
    bool _fillLight { true };
    bool _backLight { true };

    int _maxDepth { HDOSPRAY_DEFAULT_MAX_DEPTH };
    int _russianRouletteStartDepth { HDOSPRAY_DEFAULT_RR_START_DEPTH };
    float _minContribution { HDOSPRAY_DEFAULT_MIN_CONTRIBUTION };
    float _maxContribution { HDOSPRAY_DEFAULT_MAX_CONTRIBUTION };

    float _aoRadius { HDOSPRAY_DEFAULT_AO_RADIUS };
    float _aoIntensity { HDOSPRAY_DEFAULT_AO_INTENSITY };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_RENDER_PASS_H
