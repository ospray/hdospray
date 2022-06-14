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

#ifndef HDOSPRAY_MATERIAL_H
#define HDOSPRAY_MATERIAL_H

#include <pxr/imaging/hd/material.h>
#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

namespace opp = ospray::cpp;

PXR_NAMESPACE_OPEN_SCOPE

typedef std::shared_ptr<class HdStTextureResource> HdStTextureResourceSharedPtr;

/// OSPRay hdMaterial
///  supports uvtextures and ptex when pxr_oiio_plugin
///  and pxr_ptex_plugin are enabled.
///  The OSPRay representation can be accessed via
///  GetOSPRayMaterial().
class HdOSPRayMaterial final : public HdMaterial {
public:
    HdOSPRayMaterial(SdfPath const& id);

    virtual ~HdOSPRayMaterial() = default;

    /// Synchronizes state from the delegate to this object.
    virtual void Sync(HdSceneDelegate* sceneDelegate,
                      HdRenderParam* renderParam,
                      HdDirtyBits* dirtyBits) override;

    /// Returns the minimal set of dirty bits to place in the
    /// change tracker for use in the first sync of this prim.
    /// Typically this would be all dirty bits.
    virtual HdDirtyBits GetInitialDirtyBitsMask() const override
    {
        return AllDirty;
    }

    /// Causes the shader to be reloaded.
    virtual void Reload()
    {
    }

    void SetDisplayColor(GfVec4f color);

    /// Create a default material based on the renderer type specified in config
    static opp::Material CreateDefaultMaterial(GfVec4f color);

    /// Create a default material based on the renderer type specified in config
    // static OSPMaterial CreateDiffuseMaterial(GfVec4f color);

    /// Create a default material based on the renderer type specified in config
    opp::Material CreatePrincipledMaterial(std::string renderType);

    /// Create a default material based on the renderer type specified in config
    opp::Material CreateSimpleMaterial(std::string renderType);

    /// Create a default material based on the renderer type specified in config
    opp::Material CreateScivisMaterial(std::string renderType);

    /// Summary flag. Returns true if the material is bound to one or more
    /// textures and any of those textures is a ptex texture.
    /// If no textures are bound or all textures are uv textures, then
    /// the method returns false.
    inline bool HasPtex() const
    {
        return hasPtex;
    }

    // Return the OSPMaterial object generated by Sync
    inline const opp::Material GetOSPRayMaterial() const
    {
        return _ospMaterial;
    }

private:
    inline float RoughnesToPhongExponent(float roughness)
    {
        if (roughness > 0.0f) {
            return std::min(2.0f / std::pow(roughness, 4.f) - 2.0f, 1000.0f);
        } else {
            return 1000.0f;
        }
    }

    inline float EvalAvgFresnel(float ior)
    {
        if (ior >= 400.0f) {
            return 1.0f;
        } else if (1.0f == ior) {
            return 0.0f;
        } else if (1.0 < ior) {
            return (ior - 1.0f) / (4.08567f + 1.00071f * ior);
        } else {
            return 0.997118f + 0.1014 * ior - 0.965241 * ior * ior
                   - 0.130607 * ior * ior * ior;
        }
    }

protected:
    // update osp representations for material
    void _UpdateOSPRayMaterial();
    // fill in material parameters based on usdPreviewSurface node
    void _ProcessUsdPreviewSurfaceNode(HdMaterialNode node);
    // parse texture node params and set them to appropriate map_ texture var
    void _ProcessTextureNode(HdMaterialNode node, TfToken textureName);

    struct HdOSPRayTexture {
        std::string file;
        enum class WrapType { NONE, BLACK, CLAMP, REPEAT, MIRROR };
        WrapType wrapS, wrapT;
        GfVec4f scale { 1.0f };
        enum class ColorType { NONE, RGBA, RGB, R, G, B, A };
        ColorType type;
        opp::Texture ospTexture { nullptr };
        bool isPtex { false };
    };

    GfVec3f diffuseColor { 0.18f, 0.18f, 0.18f };
    GfVec3f specularColor { 0.0f, 0.0f, 0.0f };
    float metallic { 0.f };
    float roughness { 0.5f };
    float coatRoughness { 0.01f };
    float coat { 0.f };
    float ior { 1.5f };
    float opacity { 1.f };
    TfToken type;
    bool hasPtex { false };

    HdOSPRayTexture map_diffuseColor;
    HdOSPRayTexture map_diffuse;
    HdOSPRayTexture map_emissiveColor;
    HdOSPRayTexture map_specularColor;
    HdOSPRayTexture map_specular;
    HdOSPRayTexture map_metallic;
    HdOSPRayTexture map_roughness;
    HdOSPRayTexture map_coat;
    HdOSPRayTexture map_coatRoughness;
    HdOSPRayTexture map_ior;
    HdOSPRayTexture map_opacity;
    HdOSPRayTexture map_normal;
    HdOSPRayTexture map_displacement;

    opp::Material _ospMaterial;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_MATERIAL_H
