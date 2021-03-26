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

#include "pxr/imaging/hdOSPRay/material.h"
#include "pxr/imaging/hdOSPRay/texture.h"

#include "pxr/base/gf/vec3f.h"
#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/usd/sdf/assetPath.h"

#include "pxr/imaging/hdOSPRay/config.h"
#include "pxr/imaging/hdOSPRay/context.h"

#include "pxr/imaging/hdSt/material.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/shaderCode.h"
#include "pxr/imaging/hdSt/surfaceShader.h"
#include <OpenImageIO/imageio.h>

#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"

using namespace rkcommon::math;

OIIO_NAMESPACE_USING

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayMaterialTokens,
    (UsdPreviewSurface)
    (HwPtexTexture_1)
);

TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayTokens,
    (UsdPreviewSurface)
    (diffuseColor)
    (specularColor)
    (emissiveColor)
    (metallic)
    (roughness)
    (clearcoat)
    (clearcoatRoughness)
    (ior)
    (color)
    (opacity)
    (UsdUVTexture)
    (normal)
    (displacement)
    (file)
    (filename)
    (scale)
    (wrapS)
    (wrapT)
    (repeat)
    (mirror)
    (HwPtexTexture_1)
);
// clang-format on

HdOSPRayMaterial::HdOSPRayMaterial(SdfPath const& id)
    : HdMaterial(id)
{
    diffuseColor = GfVec3f(1, 1, 1);
}

/// Synchronizes state from the delegate to this object.
void
HdOSPRayMaterial::Sync(HdSceneDelegate* sceneDelegate,
                       HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    if (*dirtyBits & HdMaterial::DirtyResource) {
        // update material
        VtValue networkMapResource
               = sceneDelegate->GetMaterialResource(GetId());
        HdMaterialNetworkMap networkMap
               = networkMapResource.Get<HdMaterialNetworkMap>();
        HdMaterialNetwork matNetwork;

        if (networkMap.map.empty())
            std::cout << "material network map was empty!!!!!\n";

        // get material network from network map
        TF_FOR_ALL (itr, networkMap.map) {
            auto& network = itr->second;
            TF_FOR_ALL (node, network.nodes) {
                if (node->identifier
                    == HdOSPRayMaterialTokens->UsdPreviewSurface)
                    matNetwork = network;
            }
        }

        TF_FOR_ALL (node, matNetwork.nodes) {
            if (node->identifier == HdOSPRayTokens->UsdPreviewSurface)
                _ProcessUsdPreviewSurfaceNode(*node);
            else if (node->identifier == HdOSPRayTokens->UsdUVTexture
                     || node->identifier == HdOSPRayTokens->HwPtexTexture_1) {

                // find texture inputs and outputs
                auto relationships = matNetwork.relationships;
                auto relationship = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&node](HdMaterialRelationship const& rel) {
                           return rel.inputId == node->path;
                       });
                if (relationship == relationships.end()) {
                    continue; // node isn't actually used
                }

                TfToken texNameToken = relationship->outputName;
                _ProcessTextureNode(*node, texNameToken);
            }
        }

        _UpdateOSPRayMaterial();

        *dirtyBits = Clean;
    }
}

void
HdOSPRayMaterial::_UpdateOSPRayMaterial()
{
    std::string rendererType = HdOSPRayConfig::GetInstance().usePathTracing
           ? "pathtracer"
           : "scivis";

    if (rendererType == "pathtracer") {
        if (!HdOSPRayConfig::GetInstance().useSimpleMaterial) {
            _ospMaterial = CreatePrincipledMaterial(rendererType);
        } else {
            _ospMaterial = CreateSimpleMaterial(rendererType);
        }
    } else {
        _ospMaterial = CreateScivisMaterial(rendererType);
    }

    _ospMaterial.commit();
}

void
HdOSPRayMaterial::_ProcessUsdPreviewSurfaceNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayTokens->diffuseColor) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayTokens->metallic) {
            metallic = value.Get<float>();
        } else if (name == HdOSPRayTokens->roughness) {
            roughness = value.Get<float>();
        } else if (name == HdOSPRayTokens->ior) {
            ior = value.Get<float>();
        } else if (name == HdOSPRayTokens->color) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayTokens->opacity) {
            opacity = value.Get<float>();
        } else if (name == HdOSPRayTokens->clearcoat) {
            clearcoat = value.Get<float>();
        } else if (name == HdOSPRayTokens->clearcoatRoughness) {
            clearcoatRoughness = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessTextureNode(HdMaterialNode node, TfToken textureName)
{
    bool isPtex = node.identifier == HdOSPRayTokens->HwPtexTexture_1;

    HdOSPRayTexture texture;
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayTokens->file) {
            SdfAssetPath const& path = value.Get<SdfAssetPath>();
            texture.file = path.GetResolvedPath();
            texture.ospTexture = LoadOIIOTexture2D(texture.file);
        } else if (name == HdOSPRayTokens->filename || name.GetString() == "info:filename") {
            SdfAssetPath const& path = value.Get<SdfAssetPath>();
            texture.file = path.GetResolvedPath();
            if (isPtex) {
                hasPtex = true;
                texture.isPtex = true;
#ifdef HDOSPRAY_PLUGIN_PTEX
                texture.ospTexture = LoadPtexTexture(texture.file);
#endif
            }
        } else if (name == HdOSPRayTokens->scale) {
            texture.scale = value.Get<GfVec4f>();
        } else if (name == HdOSPRayTokens->wrapS) {
        } else if (name == HdOSPRayTokens->wrapT) {
        } else {
            std::cout << "unhandled token: " << name.GetString() << " "
                      << std::endl;
        }
    }

    if (texture.ospTexture)
        texture.ospTexture.commit();

    if (textureName == HdOSPRayTokens->diffuseColor) {
        map_diffuseColor = texture;
    } else if (textureName == HdOSPRayTokens->metallic) {
        map_metallic = texture;
    } else if (textureName == HdOSPRayTokens->roughness) {
        map_roughness = texture;
    } else if (textureName == HdOSPRayTokens->normal) {
        map_normal = texture;
    } else if (textureName == HdOSPRayTokens->opacity) {
        map_opacity = texture;
    } else
        std::cout << "unhandled texToken: " << textureName.GetString()
                  << std::endl;
}

opp::Material
HdOSPRayMaterial::CreateDefaultMaterial(GfVec4f color)
{
    std::string rendererType = HdOSPRayConfig::GetInstance().usePathTracing
           ? "pathtracer"
           : "scivis";
    opp::Material ospMaterial;
    if (rendererType == "pathtracer"
        && !HdOSPRayConfig::GetInstance().useSimpleMaterial) {
        ospMaterial = opp::Material(rendererType.c_str(), "principled");
        ospMaterial.setParam("baseColor", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("ior", 1.5f);
        ospMaterial.setParam("metallic", 0.0f);
        ospMaterial.setParam("roughness", 0.25f);
    } else {
        ospMaterial = opp::Material(rendererType.c_str(), "obj");
        // Carson: apparently colors are actually stored as a single color value
        // for entire object
        ospMaterial.setParam("ns", 10.f);
        ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
        ospMaterial.setParam("kd", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("d", color.data()[3]);
    }
    ospMaterial.commit();
    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreatePrincipledMaterial(std::string rendererType)
{

    opp::Material ospMaterial
           = opp::Material(rendererType.c_str(), "principled");
    ospMaterial.setParam("ior", ior);
    ospMaterial.setParam(
           "baseColor",
           vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    ospMaterial.setParam("metallic", metallic);
    ospMaterial.setParam("roughness", roughness);
    ospMaterial.setParam("normal", normal);
    ospMaterial.setParam("transmission", 1.0f - opacity);
    ospMaterial.setParam("transmissionDepth", 400.f);
    if (opacity < 1.0f) {
        ospMaterial.setParam(
               "transmissionColor",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    }
    if (map_diffuseColor.ospTexture) {
        ospMaterial.setParam("map_baseColor", map_diffuseColor.ospTexture);
        ospMaterial.setParam("baseColor",
                             vec3f(map_diffuseColor.scale[0],
                                   map_diffuseColor.scale[1],
                                   map_diffuseColor.scale[2]));
    }
    if (map_metallic.ospTexture) {
        ospMaterial.setParam("map_metallic", map_metallic.ospTexture);
        metallic = 1.0f;
    }
    if (map_roughness.ospTexture) {
        ospMaterial.setParam("map_roughness", map_roughness.ospTexture);
        roughness = 1.0f;
    }
    if (map_normal.ospTexture) {
        ospMaterial.setParam("map_normal", map_normal.ospTexture);
        normal = 1.f;
    }
    if (map_opacity.ospTexture) {
        ospMaterial.setParam("map_opacity", map_opacity.ospTexture);
        opacity = 1.f;
    }
    ospMaterial.commit();
    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreateSimpleMaterial(std::string rendererType)
{
    opp::Material ospMaterial = opp::Material(rendererType.c_str(), "obj");
    float avgFresnel = EvalAvgFresnel(ior);

    if (metallic == 0.f) {
        ospMaterial.setParam(
               "kd",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2])
                      * (1.0f - avgFresnel));
        ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2])
                      * avgFresnel);
        ospMaterial.setParam("ns",
                             RoughnesToPhongExponent(std::sqrt(roughness)));
    } else {
        ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2]));
        ospMaterial.setParam("ns",
                             RoughnesToPhongExponent(std::sqrt(roughness)));
    }

    if (opacity < 1.0f) {
        float tf = 1.0f - opacity;
        ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        ospMaterial.setParam(
               "tf",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]) * tf);
    }
    if (map_diffuseColor.ospTexture) {
        ospMaterial.setParam("map_kd", map_diffuseColor.ospTexture);
        diffuseColor = GfVec3f(1.0);
    }
    ospMaterial.commit();
    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreateScivisMaterial(std::string rendererType)
{
    opp::Material ospMaterial = opp::Material(rendererType.c_str(), "obj");
    // Carson: apparently colors are actually stored as a single color value
    // for entire object
    ospMaterial.setParam("ns", 10.f);
    ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
    ospMaterial.setParam(
           "kd", vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    if (map_diffuseColor.ospTexture) {
        ospMaterial.setParam("map_kd", map_diffuseColor.ospTexture);
        diffuseColor = GfVec3f(1.0);
    }
    ospMaterial.setParam("d", opacity);
    ospMaterial.commit();
    return ospMaterial;
}

PXR_NAMESPACE_CLOSE_SCOPE
