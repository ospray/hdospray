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
#ifndef HDOSPRAY_RENDER_PASS_H
#define HDOSPRAY_RENDER_PASS_H

#include "pxr/base/gf/matrix4d.h"
#include "pxr/imaging/hd/renderPass.h"
#include "pxr/pxr.h"

#include "ospray/ospray_cpp.h"
#include "rkcommon/math/vec.h"

namespace opp = ospray::cpp;

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

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
                       opp::Renderer renderer, std::atomic<int>* sceneVersion,
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

    struct RenderFrame 
    {
        opp::Future osprayFrame;
        unsigned int width{0};
        unsigned int height{0};
        // The resolved output buffer, in GL_RGBA. This is an intermediate between
        // _sampleBuffer and the GL framebuffer.
        std::vector<vec4f> colorBuffer;

        bool isValid() { return osprayFrame;}
    };

    virtual void DisplayRenderBuffer(RenderFrame& renderFrame);

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

private:
    // -----------------------------------------------------------------------
    // Internal API

    // Specify a new viewport size for the sample buffer. Note: the caller
    // should also call ResetImage().
    void _ResizeSampleBuffer(unsigned int width, unsigned int height);

    // The sample buffer is cleared in Execute(), so this flag records whether
    // ResetImage() has been called since the last Execute().
    bool _pendingResetImage;
    bool _pendingModelUpdate;

    opp::FrameBuffer _frameBuffer;
    opp::FrameBuffer _interactiveFrameBuffer;
    int _interactiveFrameBufferScale {2};

    opp::Renderer _renderer;

    bool _interacting {true};
    bool _interactingLastFrame {true};

    // A reference to the global scene version.
    std::atomic<int>* _sceneVersion;
    // The last scene version we rendered with.
    int _lastRenderedVersion { -1 };
    int _lastRenderedModelVersion { -1 };
    int _lastSettingsVersion { -1 };


    RenderFrame _currentFrame;
    RenderFrame _previousFrame;

    // The width of the viewport we're rendering into.
    unsigned int _width;
    // The height of the viewport we're rendering into.
    unsigned int _height;

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
    int _spp { 1 };
    bool _useDenoiser { false };
    bool _denoiserState { false };
    OSPPixelFilterTypes _pixelFilterType {
        OSPPixelFilterTypes::OSP_PIXELFILTER_GAUSS
    };
    int _samplesToConvergence { 100 };
    int _denoiserSPPThreshold { 6 };
    int _aoSamples { 1 };
    int _lightSamples { -1 };
    bool _staticDirectionalLights { true };
    bool _ambientLight { true };
    bool _eyeLight { true };
    bool _keyLight { true };
    bool _fillLight { true };
    bool _backLight { true };
    int _maxDepth { 5 };
    float _minContribution {0.1f};
    float _maxContribution {3.0f};
    float _aoDistance { 10.f };
    float _aoIntensity { 1.f };

    // OpenGL framebuffer texture
    GLuint framebufferTexture = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_RENDER_PASS_H
