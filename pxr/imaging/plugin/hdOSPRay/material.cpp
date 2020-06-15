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

#include "ospray/ospray_util.h"
#include "rkcommon/math/vec.h"

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

HdOSPRayMaterial::~HdOSPRayMaterial()
{
    if (_ospMaterial)
        ospRelease(_ospMaterial);
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
    if (_ospMaterial)
        ospRelease(_ospMaterial);

    _ospMaterial = CreateDefaultMaterial(
           { diffuseColor[0], diffuseColor[1], diffuseColor[2], 1.f });

    ospSetFloat(_ospMaterial, "ior", ior);
    ospSetVec3f(_ospMaterial, "baseColor", diffuseColor[0], diffuseColor[1],
                diffuseColor[2]);
    ospSetFloat(_ospMaterial, "metallic", metallic);
    ospSetFloat(_ospMaterial, "roughness", roughness);
    ospSetFloat(_ospMaterial, "normal", normal);

    if (map_diffuseColor.ospTexture) {
        ospSetObject(_ospMaterial, "map_baseColor",
                     map_diffuseColor.ospTexture);
        ospSetObject(_ospMaterial, "map_kd", map_diffuseColor.ospTexture);
    }
    if (map_metallic.ospTexture) {
        ospSetObject(_ospMaterial, "map_metallic", map_metallic.ospTexture);
        metallic = 1.0f;
    }
    if (map_roughness.ospTexture) {
        ospSetObject(_ospMaterial, "map_roughness", map_roughness.ospTexture);
        roughness = 1.0f;
    }
    if (map_normal.ospTexture) {
        ospSetObject(_ospMaterial, "map_normal", map_normal.ospTexture);
        normal = 1.f;
    }
    ospCommit(_ospMaterial);
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
        } else if (name == HdOSPRayTokens->filename) {
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
        ospCommit(texture.ospTexture);

    if (textureName == HdOSPRayTokens->diffuseColor) {
        map_diffuseColor = texture;
    } else if (textureName == HdOSPRayTokens->metallic) {
        map_metallic = texture;
    } else if (textureName == HdOSPRayTokens->roughness) {
        map_roughness = texture;
    } else if (textureName == HdOSPRayTokens->normal) {
        map_normal = texture;
    } else
        std::cout << "unhandled texToken: " << textureName.GetString()
                  << std::endl;
}

OSPMaterial
HdOSPRayMaterial::CreateDefaultMaterial(GfVec4f color)
{
    std::string rendererType = HdOSPRayConfig::GetInstance().usePathTracing
           ? "pathtracer"
           : "scivis";
    OSPMaterial ospMaterial;
    if (rendererType == "pathtracer") {
        ospMaterial = ospNewMaterial(rendererType.c_str(), "principled");
        ospSetVec3f(ospMaterial, "baseColor", color[0], color[1], color[2]);
    } else {
        ospMaterial = ospNewMaterial(rendererType.c_str(), "obj");
        // Carson: apparently colors are actually stored as a single color value
        // for entire object
        ospSetFloat(ospMaterial, "ns", 10.f);
        ospSetVec3f(ospMaterial, "ks", 0.2f, 0.2f, 0.2f);
        ospSetVec3f(ospMaterial, "kd", color[0], color[1], color[2]);
        ospSetFloat(ospMaterial, "d", color.data()[3]);
    }
    ospCommit(ospMaterial);
    return ospMaterial;
}

PXR_NAMESPACE_CLOSE_SCOPE
