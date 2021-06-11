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
#ifndef HDOSPRAY_RENDER_DELEGATE_H
#define HDOSPRAY_RENDER_DELEGATE_H

#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/pxr.h>

#include "api.h"

#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"

#include <mutex>

namespace opp = ospray::cpp;

PXR_NAMESPACE_OPEN_SCOPE

#define HDOSPRAY_RENDER_SETTINGS_TOKENS                                        \
    (ambientOcclusionSamples)(samplesPerFrame)(lightSamples)(useDenoiser)(     \
           pixelFilterType)(maxDepth)(russianRouletteStartDepth)(aoRadius)(    \
           aoIntensity)(samplesToConvergence)(ambientLight)(eyeLight)(         \
           keyLight)(fillLight)(backLight)(pathTracer)(                        \
           staticDirectionalLights)(minContribution)(maxContribution)(         \
           interactiveTargetFPS)(useTextureGammaCorrection)

TF_DECLARE_PUBLIC_TOKENS(HdOSPRayRenderSettingsTokens,
                         HDOSPRAY_RENDER_SETTINGS_TOKENS);

#define HDOSPRAY_TOKENS (ospray)(glslfx)

TF_DECLARE_PUBLIC_TOKENS(HdOSPRayTokens, HDOSPRAY_API, HDOSPRAY_TOKENS);

class HdOSPRayRenderParam;

///
/// \class HdOSPRayRenderDelegate
///
/// Manages OSPRay specific objects mapped from Hydra Rprims, Sprims,
/// and Bprims.
///
/// Primitives in Hydra are split into Rprims (drawables), Sprims (state
/// objects like cameras and materials), and Bprims (buffer objects like
/// textures). The minimum set of primitives a renderer needs to support is
/// one Rprim (so the scene's not empty) and the "camera" Sprim, which is
/// required by HdRenderTask, the task implementing basic hydra drawing.
///
///
class HdOSPRayRenderDelegate final : public HdRenderDelegate {
public:
    /// Render delegate constructor. This method creates the RTC device and
    /// scene, and links OSPRay error handling to hydra error handling.
    HdOSPRayRenderDelegate();

    /// Render delegate constructor. This method creates the RTC device and
    /// scene, and links OSPRay error handling to hydra error handling.
    //  Sets render settings.
    HdOSPRayRenderDelegate(HdRenderSettingsMap const& settingsMap);

    /// Render delegate destructor. This method destroys the RTC device and
    /// scene.
    virtual ~HdOSPRayRenderDelegate();

    /// Return this delegate's render param.
    ///   \return A shared instance of HdOSPRayRenderParam.
    virtual HdRenderParam* GetRenderParam() const override;

    /// Return a list of which Rprim types can be created by this class's
    /// CreateRprim.
    virtual const TfTokenVector& GetSupportedRprimTypes() const override;
    /// Return a list of which Sprim types can be created by this class's
    /// CreateSprim.
    virtual const TfTokenVector& GetSupportedSprimTypes() const override;
    /// Return a list of which Bprim types can be created by this class's
    /// CreateBprim.
    virtual const TfTokenVector& GetSupportedBprimTypes() const override;

    /// Returns the HdResourceRegistry instance used by this render delegate.
    virtual HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    /// Create a renderpass. Hydra renderpasses are responsible for drawing
    /// a subset of the scene (specified by the "collection" parameter) to the
    /// current framebuffer. This class creates objects of type
    /// HdOSPRayRenderPass, which draw using OSPRay's raycasting API.
    ///   \param index The render index this renderpass will be bound to.
    ///   \param collection A specifier for which parts of the scene should
    ///                     be drawn.
    ///   \return An OSPRay renderpass object.
    virtual HdRenderPassSharedPtr
    CreateRenderPass(HdRenderIndex* index,
                     HdRprimCollection const& collection) override;

    /// Create an instancer. Hydra instancers store data needed for an
    /// instanced object to draw itself multiple times.
    ///   \param delegate The scene delegate providing data for this
    ///                   instancer.
    ///   \param id The scene graph ID of this instancer, used when pulling
    ///             data from a scene delegate.
    ///   \param instancerId If specified, the instancer at this id uses
    ///                      this instancer as a prototype.
    ///   \return An OSPRay instancer object.
    virtual HdInstancer* CreateInstancer(HdSceneDelegate* delegate,
                                         SdfPath const& id,
                                         SdfPath const& instancerId);

    /// Destroy an instancer created with CreateInstancer.
    ///   \param instancer The instancer to be destroyed.
    virtual void DestroyInstancer(HdInstancer* instancer);

    /// Create an OSPRay specific hydra Rprim, representing scene geometry.
    ///   \param typeId The rprim type to create. This must be one of the types
    ///                 from GetSupportedRprimTypes().
    ///   \param rprimId The scene graph ID of this rprim, used when pulling
    ///                  data from a scene delegate.
    ///   \param instancerId If specified, the instancer at this id uses the
    ///                      new rprim as a prototype.
    ///   \return An OSPRay rprim object.
    virtual HdRprim* CreateRprim(TfToken const& typeId, SdfPath const& rprimId,
                                 SdfPath const& instancerId) override;

    /// Destroy an Rprim created with CreateRprim.
    ///   \param rPrim The rprim to be destroyed.
    virtual void DestroyRprim(HdRprim* rPrim) override;

    /// Create a hydra Sprim, representing scene or viewport state like cameras
    /// or lights.
    ///   \param typeId The sprim type to create. This must be one of the types
    ///                 from GetSupportedSprimTypes().
    ///   \param sprimId The scene graph ID of this sprim, used when pulling
    ///                  data from a scene delegate.
    ///   \return An OSPRay sprim object.
    virtual HdSprim* CreateSprim(TfToken const& typeId,
                                 SdfPath const& sprimId) override;

    /// Create a hydra Sprim using default values, and with no scene graph
    /// binding.
    ///   \param typeId The sprim type to create. This must be one of the types
    ///                 from GetSupportedSprimTypes().
    ///   \return An OSPRay fallback sprim object.
    virtual HdSprim* CreateFallbackSprim(TfToken const& typeId) override;

    /// Destroy an Sprim created with CreateSprim or CreateFallbackSprim.
    ///   \param sPrim The sprim to be destroyed.
    virtual void DestroySprim(HdSprim* sPrim) override;

    /// Create a hydra Bprim, representing data buffers such as textures.
    ///   \param typeId The bprim type to create. This must be one of the types
    ///                 from GetSupportedBprimTypes().
    ///   \param bprimId The scene graph ID of this bprim, used when pulling
    ///                  data from a scene delegate.
    ///   \return An OSPRay bprim object.
    virtual HdBprim* CreateBprim(TfToken const& typeId,
                                 SdfPath const& bprimId) override;

    /// Create a hydra Bprim using default values, and with no scene graph
    /// binding.
    ///   \param typeId The bprim type to create. This must be one of the types
    ///                 from GetSupportedBprimTypes().
    ///   \return An OSPRay fallback bprim object.
    virtual HdBprim* CreateFallbackBprim(TfToken const& typeId) override;

    /// Destroy a Bprim created with CreateBprim or CreateFallbackBprim.
    ///   \param bPrim The bprim to be destroyed.
    virtual void DestroyBprim(HdBprim* bPrim) override;

    /// This function is called after new scene data is pulled during prim
    /// Sync(), but before any tasks (such as draw tasks) are run, and gives the
    /// render delegate a chance to transfer any invalidated resources to the
    /// rendering kernel.
    ///   \param tracker The change tracker passed to prim Sync().
    virtual void CommitResources(HdChangeTracker* tracker) override;

    HDOSPRAY_API
    virtual TfToken GetMaterialNetworkSelector() const;

    /// This function tells the scene which material variant to reference.
    ///   \return A token specifying which material variant this renderer
    ///           prefers.
    virtual TfToken GetMaterialBindingPurpose() const override
    {
        return HdTokens->full;
    }

    /// This function returns the default AOV descriptor for a given named AOV.
    /// This mechanism lets the renderer decide things like what format
    /// a given AOV will be written as.
    ///   \param name The name of the AOV whose descriptor we want.
    ///   \return A descriptor specifying things like what format the AOV
    ///           output buffer should be.
    virtual HdAovDescriptor
    GetDefaultAovDescriptor(TfToken const& name) const override;

    /// Returns a list of user-configurable render settings.
    /// This is a reflection API for the render settings dictionary; it need
    /// not be exhaustive, but can be used for populating application settings
    /// UI.
    virtual HdRenderSettingDescriptorList
    GetRenderSettingDescriptors() const override;

private:
    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    /// Resource registry used in this render delegate
    static std::mutex _mutexResourceRegistry;
    static std::atomic_int _counterResourceRegistry;
    static HdResourceRegistrySharedPtr _resourceRegistry;

    // This class does not support copying.
    HdOSPRayRenderDelegate(const HdOSPRayRenderDelegate&) = delete;
    HdOSPRayRenderDelegate& operator=(const HdOSPRayRenderDelegate&) = delete;

    // constructor helper
    void _Initialize();

    opp::Renderer
           _renderer; // moved from Pass to Delegate due to Material dependancy

    // A list of render setting exports.
    HdRenderSettingDescriptorList _settingDescriptors;

    int _lastCommittedModelVersion { -1 };

    // A shared HdOSPRayRenderParam object that stores top-level OSPRay state;
    // passed to prims during Sync().
    std::shared_ptr<HdOSPRayRenderParam> _renderParam;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_RENDER_DELEGATE_H
