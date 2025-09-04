// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "material.h"
#include "renderParam.h"
#include "texture.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>

#include "config.h"
#include "context.h"

#include <pxr/imaging/hdSt/material.h>
#include <pxr/imaging/hdSt/resourceRegistry.h>
#include <pxr/imaging/hdSt/shaderCode.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

using namespace rkcommon::math;

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayMaterialTokens,
    (UsdPreviewSurface)
    (HwPtexTexture_1)
    (osp)
    (OspPrincipled)
    (OspCarPaint)
    (OspLuminous)
    (OspThinGlass)
    (OspGlass)
    (UsdTransform2d)
    (sourceColorSpace)
    (diffuseColor)
    (specularColor)
    (emissiveColor)
    (metallic)
    (map_metallic)
    (roughness)
    (map_roughness)
    (clearcoat)
    (clearcoatRoughness)
    (ior)
    (map_ior)
    (eta)
    (color)
    (baseColor)
    (map_baseColor)
    (attenuationColor)
    (attenuationDistance)
    (opacity)
    (map_opacity)
    (UsdUVTexture)
    (normal)
    (map_normal)
    (baseNormal)
    (map_baseNormal)
    (displacement)
    (file)
    (filename)
    ( (infoFilename, "info:filename"))
    (scale)
    (wrapS)
    (wrapT)
    (fallback)
    (bias)
    (st)
    (repeat)
    (mirror)
    (rotation)
    (map_rotation)
    (translation)
    (diffuse)
    (map_diffuse)
    (coat)
    (coatColor)
    (coatThickness)
    (coatRoughness)
    (coatNormal)
    (coatIor)
    (flipflopColor)
    (flipflopFalloff)
    (sheen)
    (map_sheen)
    (sheenColor)
    (sheenTint)
    (sheenRoughness)
    (transmission)
    (map_transmission)
    (transmissionColor)
    (transmissionDepth)
    (anisotropy)
    (flakeColor)
    (flakeDensity)
    (flakeScale)
    (flakeSpread)
    (flakeJitter)
    (flakeRoughness)
    (thickness)
    (thin)
    (backlight)
    (intensity)
    (edgeColor)
    (map_edgeColor)
    (specular)
    (map_specular)
    (transparency)
);

// clang-format on

static vec3f
GfToOSP(const GfVec3f& v)
{
    return vec3f(v[0], v[1], v[2]);
}

HdOSPRayMaterial::HdOSPRayMaterial(SdfPath const& id)
    : HdMaterial(id)
{
    diffuseColor = GfVec3f(1, 1, 1);
}

void
HdOSPRayMaterial::Sync(HdSceneDelegate* sceneDelegate,
                       HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);

    // if material dirty, update
    if (*dirtyBits & HdMaterial::DirtyResource) {
        //  find material network
        VtValue networkMapResource
               = sceneDelegate->GetMaterialResource(GetId());
        HdMaterialNetworkMap networkMap
               = networkMapResource.Get<HdMaterialNetworkMap>();
        HdMaterialNetwork matNetwork;

        TF_FOR_ALL (itr, networkMap.map) {
            auto& network = itr->second;
            TF_FOR_ALL (node, network.nodes) {
                MaterialTypes newType = MaterialTypes::preview;
                if (node->identifier
                    == HdOSPRayMaterialTokens->UsdPreviewSurface) {
                    matNetwork = network;
                    newType = MaterialTypes::preview;
                } else if (node->identifier
                           == HdOSPRayMaterialTokens->OspPrincipled) {
                    matNetwork = network;
                    newType = MaterialTypes::principled;
                    roughness = 0.f; // default roughness
                } else if (node->identifier
                           == HdOSPRayMaterialTokens->OspCarPaint) {
                    matNetwork = network;
                    newType = MaterialTypes::carPaint;
                    roughness = 0.f; // default roughness
                } else if (node->identifier
                           == HdOSPRayMaterialTokens->OspLuminous) {
                    matNetwork = network;
                    newType = MaterialTypes::luminous;
                } else if (node->identifier
                           == HdOSPRayMaterialTokens->OspThinGlass) {
                    matNetwork = network;
                    newType = MaterialTypes::thinGlass;
                } else if (node->identifier
                           == HdOSPRayMaterialTokens->OspGlass) {
                    matNetwork = network;
                    newType = MaterialTypes::glass;
                }
                if (newType != _type)
                    _typeDirty = true;
                _type = newType;
            }
        }

        // process each material node based on type
        TF_FOR_ALL (node, matNetwork.nodes) {
            if (node->identifier == HdOSPRayMaterialTokens->UsdPreviewSurface) {
                _ProcessUsdPreviewSurfaceNode(*node);
            } else if (node->identifier == HdOSPRayMaterialTokens->OspPrincipled) {
                _ProcessOspPrincipledNode(*node);
            } else if (node->identifier == HdOSPRayMaterialTokens->OspCarPaint)
                _ProcessOspCarPaintNode(*node);
            else if (node->identifier == HdOSPRayMaterialTokens->OspLuminous)
                _ProcessOspLuminousNode(*node);
            else if (node->identifier == HdOSPRayMaterialTokens->OspThinGlass)
                _ProcessOspThinGlassNode(*node);
            else if (node->identifier == HdOSPRayMaterialTokens->OspGlass)
                _ProcessOspGlassNode(*node);
            else if (node->identifier == HdOSPRayMaterialTokens->UsdUVTexture
                     || node->identifier
                            == HdOSPRayMaterialTokens->HwPtexTexture_1) {
                // texture node found

                // determine if texture is actively used
                auto relationships = matNetwork.relationships;
                auto relationship = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&node](HdMaterialRelationship const& rel) {
                           return rel.inputId == node->path;
                       });
                if (relationship == relationships.end()) {
                    continue; // ignore unused texture
                }

                const TfToken inputNameToken = relationship->inputName;
                const TfToken texNameToken = relationship->outputName;
                _ProcessTextureNode(*node, inputNameToken, texNameToken);
            } else if (node->identifier
                       == HdOSPRayMaterialTokens->UsdTransform2d) {
                // calculate transform2d to be used on a texture
                auto relationships = matNetwork.relationships;
                auto relationship = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&node](HdMaterialRelationship const& rel) {
                           return rel.inputId == node->path;
                       });
                if (relationship == relationships.end()) {
                    continue; // skip unused nodes
                }

                TfToken texNameToken = relationship->outputName;

                // node is used, find what param it representds, ie diffuseColor
                auto relationship2 = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&relationship](HdMaterialRelationship const& rel) {
                           return rel.inputId == relationship->outputId;
                       });
                if (relationship2 == relationships.end()) {
                    continue; // skip unused nodes
                }
                _ProcessTransform2dNode(*node, relationship2->outputName);
            }
        }

        _UpdateOSPRayMaterial();

        ospRenderParam->UpdateMaterialVersion();
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
            if (_type == MaterialTypes::carPaint) {
                if (_typeDirty)
                    _ospMaterial = opp::Material("carPaint");
                UpdateCarPaintMaterial();
            } else if (_type == MaterialTypes::luminous) {
                if (_typeDirty)
                    _ospMaterial = opp::Material("luminous");
                UpdateLuminousMaterial();
            } else if (_type == MaterialTypes::thinGlass) {
                if (_typeDirty)
                    _ospMaterial = opp::Material("thinGlass");
                UpdateThinGlassMaterial();
            } else if (_type == MaterialTypes::glass) {
                if (_typeDirty)
                    _ospMaterial = opp::Material("glass");
                UpdateGlassMaterial();
            } else { // preview, principled, other
                if (_typeDirty)
                    _ospMaterial = opp::Material("principled");
                UpdatePrincipledMaterial(rendererType);
            }
        } else {
            if (_typeDirty)
                _ospMaterial = opp::Material("obj");
            UpdateSimpleMaterial(rendererType);
        }
        _typeDirty = false;
    } else {
        if (_typeDirty)
            _ospMaterial = opp::Material("obj");
        UpdateScivisMaterial(rendererType);
    }

    _ospMaterial.commit();
}

void
HdOSPRayMaterial::_ProcessUsdPreviewSurfaceNode(HdMaterialNode node)
{
    thin = true; // thin default for usdpreview
    ior = 1.5; // ior has 1.5 default for usdpreview
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->diffuseColor) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->emissiveColor) {
            emissiveColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->specularColor) {
            specularColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->metallic) {
            metallic = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->roughness) {
            roughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->ior) {
            ior = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->clearcoatRoughness) {
            coatRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->clearcoat) {
            coat = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->opacity) {
            opacity = value.Get<float>();
        }
    }
    // set transmissionColor to diffuseColor for usdpreviewsurface as there is
    // no way to set it
    transmissionColor = diffuseColor;
}

void
HdOSPRayMaterial::_ProcessOspPrincipledNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->baseColor) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->edgeColor) {
            edgeColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->transmissionColor) {
            transmissionColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->transmissionDepth) {
            transmissionDepth = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->anisotropy) {
            anisotropy = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->rotation) {
            rotation = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->thin) {
            if (value.IsHolding<float>())
                thin = value.Get<float>();
            else if (value.IsHolding<bool>())
                thin = value.Get<bool>();
        } else if (name == HdOSPRayMaterialTokens->thickness) {
            thickness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->backlight) {
            backlight = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coat) {
            coat = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatColor) {
            coatColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->coatThickness) {
            coatThickness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatRoughness) {
            coatRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatNormal) {
            coatNormal = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->sheen) {
            sheen = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->sheenColor) {
            sheenColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->sheenTint) {
            sheenTint = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->sheenRoughness) {
            sheenRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->transmission) {
            transmission = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->opacity) {
            opacity = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->specularColor) {
            specularColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->metallic) {
            metallic = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->diffuse) {
            diffuse = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->specular) {
            specular = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->normal) {
            normalScale = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->baseNormal) {
            baseNormal = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->roughness) {
            roughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->ior) {
            ior = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessOspCarPaintNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->baseColor) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->normal) {
            normalScale = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->roughness) {
            roughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flakeColor) {
            flakeColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->flakeDensity) {
            flakeDensity = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flakeScale) {
            flakeScale = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flakeSpread) {
            flakeSpread = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flakeJitter) {
            flakeJitter = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flakeRoughness) {
            flakeRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coat) {
            coat = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatIor) {
            coatIor = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatColor) {
            coatColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->coatThickness) {
            coatThickness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatRoughness) {
            coatRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->coatNormal) {
            coatNormal = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->flipflopColor) {
            flipflopColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->flipflopFalloff) {
            flipflopFalloff = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessOspLuminousNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->color) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->intensity) {
            intensity = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->transparency) {
            opacity = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessOspThinGlassNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->attenuationColor) {
            attenuationColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->eta) {
            eta = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->attenuationDistance) {
            attenuationDistance = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessOspGlassNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->attenuationColor) {
            attenuationColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->eta) {
            eta = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->attenuationDistance) {
            attenuationDistance = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessTextureNode(HdMaterialNode node,
                                      const TfToken& inputName,
                                      const TfToken& outputName)
{
    bool isPtex = node.identifier == HdOSPRayMaterialTokens->HwPtexTexture_1;
    bool isUdim = false;

    if (_textures.find(outputName) == _textures.end())
        _textures[outputName] = HdOSPRayTexture();
    HdOSPRayTexture& texture = _textures[outputName];
    std::string&& filename("");
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->file
            || name == HdOSPRayMaterialTokens->filename
            || name == HdOSPRayMaterialTokens->infoFilename) {
            SdfAssetPath const& path = value.Get<SdfAssetPath>();
            filename = path.GetResolvedPath();
        } else if (name == HdOSPRayMaterialTokens->scale) {
            texture.scale = value.Get<GfVec4f>();
            texture.xfm_scale = GfVec2f(texture.scale[0], texture.scale[1]);
            texture.hasXfm = true;
        } else if (name == HdOSPRayMaterialTokens->wrapS) {
        } else if (name == HdOSPRayMaterialTokens->wrapT) {
        } else if (name == HdOSPRayMaterialTokens->st) {
        } else if (name == HdOSPRayMaterialTokens->bias) {
        } else if (name == HdOSPRayMaterialTokens->fallback) {
            fallback = value.Get<GfVec4f>();
        } else if (name == HdOSPRayMaterialTokens->sourceColorSpace) {
        }
    }

    if (filename != "") {
        texture.file = std::move(filename);
        isUdim = TfStringContains(texture.file, "<UDIM>");
        if (isPtex) {
            hasPtex = true;
            texture.isPtex = true;
#ifdef HDOSPRAY_PLUGIN_PTEX
            texture.ospTexture = LoadPtexTexture(texture.file);
#endif
        } else if (isUdim) {
            int numX, numY;
            const auto& result = LoadUDIMTexture2D(
                   texture.file, numX, numY, false,
                   (outputName == HdOSPRayMaterialTokens->opacity));
            texture.ospTexture = result.ospTexture;
            texture.data = result.data;
            texture.hasXfm = true;
            texture.xfm_scale = { 1.f / float(numX), 1.f / float(numY) };
            // OSPRay scales around the center (0.5, 0.5).  translate
            // texture from (0.5, 0.5) to (0,0)
            texture.xfm_translation
                   = { -(.5f - .5f / float(numX)), -(.5f - .5f / float(numY)) };
        } else {
            const auto& result = LoadHioTexture2D(
                   texture.file, inputName.GetString(), false,
                   (outputName == HdOSPRayMaterialTokens->opacity
                    && _type == MaterialTypes::preview));
            texture.ospTexture = result.ospTexture;
            texture.data = result.data;
        }
    }

    if (texture.ospTexture)
        texture.ospTexture.commit();
}

void
HdOSPRayMaterial::_ProcessTransform2dNode(HdMaterialNode node,
                                          TfToken textureName)
{
    if (_textures.find(textureName) == _textures.end())
        _textures[textureName] = HdOSPRayTexture();
    HdOSPRayTexture& texture = _textures[textureName];
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->scale) {
            texture.xfm_scale = value.Get<GfVec2f>();
            texture.hasXfm = true;
        } else if (name == HdOSPRayMaterialTokens->rotation) {
            texture.xfm_rotation = value.Get<float>();
            texture.hasXfm = true;
        } else if (name == HdOSPRayMaterialTokens->translation) {
            texture.xfm_translation = value.Get<GfVec2f>();
            texture.hasXfm = true;
        }
    }
}

void
HdOSPRayMaterial::SetDisplayColor(GfVec4f color)
{
    diffuseColor = GfVec3f(color[0], color[1], color[2]);
    opacity = color[3];
    _UpdateOSPRayMaterial();
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
        ospMaterial = opp::Material("principled");
        ospMaterial.setParam("baseColor", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("ior", 1.5f);
        ospMaterial.setParam("metallic", 0.0f);
        // note: setting roughness here crashing on mac
    } else {
        ospMaterial = opp::Material("obj");
        ospMaterial.setParam("ns", 10.f);
        ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
        ospMaterial.setParam("kd", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("d", color.data()[3]);
    }
    ospMaterial.commit();
    return ospMaterial;
}

void
HdOSPRayMaterial::UpdatePrincipledMaterial(const std::string& rendererType)
{
    _ospMaterial.setParam(
           "baseColor",
           vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    bool hasMetallicTex = false;
    bool hasRoughnessTex = false;
    bool hasOpacityTex = false;
    bool hasEmissiveTex = false;
    // set texture maps
    // TODO: we can skip the name mapping if we require them to be in ospray nomenclature
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->diffuseColor
            || key == HdOSPRayMaterialTokens->baseColor
            || key == HdOSPRayMaterialTokens->map_baseColor) {
            name = "map_baseColor";
        } else if (key == HdOSPRayMaterialTokens->edgeColor
            || key == HdOSPRayMaterialTokens->map_edgeColor) {
            name = "map_edgeColor";
        } else if (key == HdOSPRayMaterialTokens->ior
            || key == HdOSPRayMaterialTokens->map_ior) {
            name = "map_ior";
        } else if (key == HdOSPRayMaterialTokens->metallic
            || key == HdOSPRayMaterialTokens->map_metallic) {
            name = "map_metallic";
            hasMetallicTex = true;
        } else if (key == HdOSPRayMaterialTokens->roughness
            || key == HdOSPRayMaterialTokens->map_roughness) {
            name = "map_roughness";
        } else if (key == HdOSPRayMaterialTokens->diffuse
            || key == HdOSPRayMaterialTokens->map_diffuse) {
            name = "map_diffuse";
        } else if (key == HdOSPRayMaterialTokens->ior
            || key == HdOSPRayMaterialTokens->map_ior) {
            name = "map_ior";
        } else if (key == HdOSPRayMaterialTokens->specular
            || key == HdOSPRayMaterialTokens->map_specular) {
            name = "map_specular";
        } else if (key == HdOSPRayMaterialTokens->sheen
            || key == HdOSPRayMaterialTokens->map_sheen) {
            name = "map_sheen";
        } else if (key == HdOSPRayMaterialTokens->normal
            || key == HdOSPRayMaterialTokens->map_normal) {
            name = "map_normal";
        } else if (key == HdOSPRayMaterialTokens->baseNormal
            || key == HdOSPRayMaterialTokens->map_baseNormal) {
            name = "map_baseNormal";
        } else if (key == HdOSPRayMaterialTokens->rotation
            || key == HdOSPRayMaterialTokens->map_rotation) {
            name = "map_rotation";
        } else if (key == HdOSPRayMaterialTokens->opacity
            || key == HdOSPRayMaterialTokens->map_opacity) {
            if (_type == MaterialTypes::preview) {
                name = "map_transmission";
                hasOpacityTex = true;
            } else
                name = "map_opacity";
        } else if (key == HdOSPRayMaterialTokens->transmission
            || key == HdOSPRayMaterialTokens->map_transmission) {
            name = "map_transmission";
            hasOpacityTex = true;
        } else if (key == HdOSPRayMaterialTokens->roughness
            || key == HdOSPRayMaterialTokens->map_roughness) {
            name = "map_roughness";
            hasRoughnessTex = true;
        } else if (key == HdOSPRayMaterialTokens->emissiveColor) {
            name = "map_emissiveColor";
            hasEmissiveTex = true;
        }

        if (name != "") {
            if (value.ospTexture) {
                _ospMaterial.setParam(name.c_str(), value.ospTexture);
                if (value.hasXfm) {
                    std::string st_translation = name + ".translation";
                    std::string st_scale = name + ".scale";
                    std::string st_rotation = name + ".rotation";
                    _ospMaterial.setParam(st_translation.c_str(),
                                          vec2f(-value.xfm_translation[0],
                                                -value.xfm_translation[1]));
                    _ospMaterial.setParam(st_scale.c_str(),
                                          vec2f(value.xfm_scale[0],
                                                value.xfm_scale[1]));
                    _ospMaterial.setParam(st_rotation.c_str(),
                                          -value.xfm_rotation);
                }
            }
            // no fallbacks for principled, as you can specify both a map_ and a non map param
        }
    }

    // set material params
    if (_type == MaterialTypes::preview) {
        transmission = 1.f - opacity;
    }
    _ospMaterial.setParam("metallic", (hasMetallicTex ? 1.0f : metallic));
    _ospMaterial.setParam("roughness", (hasRoughnessTex ? 1.0f : roughness));
    _ospMaterial.setParam("coatRoughness", coatRoughness);
    _ospMaterial.setParam("ior", ior);
    _ospMaterial.setParam("transmission",
                          (hasOpacityTex ? 1.0f : transmission));
    _ospMaterial.setParam("opacity", opacity);
    _ospMaterial.setParam("thin", thin);
    _ospMaterial.setParam("diffuse", diffuse);
    _ospMaterial.setParam("specular", specular);
    _ospMaterial.setParam("transmissionColor", GfToOSP(transmissionColor));
    _ospMaterial.setParam("transmissionDepth", transmissionDepth);
    _ospMaterial.setParam("anisotropy", anisotropy);
    _ospMaterial.setParam("rotation", rotation);
    _ospMaterial.setParam("normal", normalScale);
    _ospMaterial.setParam("baseNormal", baseNormal);
    _ospMaterial.setParam("thickness", thickness);
    _ospMaterial.setParam("backlight", backlight);
    _ospMaterial.setParam("coat", coat);
    _ospMaterial.setParam("coatIor", coatIor);
    _ospMaterial.setParam("coatColor", GfToOSP(coatColor));
    _ospMaterial.setParam("coatThickness", coatThickness);
    _ospMaterial.setParam("coatRoughness", coatRoughness);
    _ospMaterial.setParam("coatNormal", coatNormal);
    _ospMaterial.setParam("sheen", sheen);
    _ospMaterial.setParam("sheenColor", GfToOSP(sheenColor));
    _ospMaterial.setParam("sheenTint", sheenTint);
    _ospMaterial.setParam("sheenRoughness", sheenRoughness);
    _ospMaterial.setParam("emissiveColor",
                          (hasEmissiveTex ? vec3f( 1.f, 1.f, 1.f) : GfToOSP(emissiveColor)));
}

void
HdOSPRayMaterial::UpdateCarPaintMaterial()
{
    _ospMaterial.setParam(
           "baseColor",
           vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    bool hasMetallicTex = false;
    bool hasRoughnessTex = false;
    bool hasOpacityTex = false;
    // set texture maps
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->diffuseColor
            || key == HdOSPRayMaterialTokens->map_baseColor
            || key == HdOSPRayMaterialTokens->baseColor) {
            name = "map_baseColor";
        } else if (key == HdOSPRayMaterialTokens->roughness
            || key == HdOSPRayMaterialTokens->map_roughness) {
            name = "map_roughness";
            hasRoughnessTex = true;
        }

        if (name != "") {
            _ospMaterial.setParam(name.c_str(), value.ospTexture);
            if (value.hasXfm) {
                std::string st_translation = name + ".translation";
                std::string st_scale = name + ".scale";
                std::string st_rotation = name + ".rotation";
                _ospMaterial.setParam(st_translation.c_str(),
                                      vec2f(-value.xfm_translation[0],
                                            -value.xfm_translation[1]));
                _ospMaterial.setParam(st_scale.c_str(),
                                      vec2f(value.xfm_scale[0],
                                            value.xfm_scale[1]));
                _ospMaterial.setParam(st_rotation.c_str(), -value.xfm_rotation);
            }
        }
    }

    // set material params
    _ospMaterial.setParam("roughness", (hasRoughnessTex ? 1.0f : roughness));
    _ospMaterial.setParam("coat", coat);
    _ospMaterial.setParam("coatIor", coatIor);
    _ospMaterial.setParam("coatColor", GfToOSP(coatColor));
    _ospMaterial.setParam("coatRoughness", coatRoughness);
    _ospMaterial.setParam("coatThickness", coatThickness);
    _ospMaterial.setParam("coatNormal", coatNormal);
    _ospMaterial.setParam("flipflopColor", GfToOSP(flipflopColor));
    _ospMaterial.setParam("flipflopFalloff", flipflopFalloff);
    _ospMaterial.setParam("flakeColor", GfToOSP(flakeColor));
    _ospMaterial.setParam("flakeDensity", flakeDensity);
    _ospMaterial.setParam("flakeScale", flakeScale);
    _ospMaterial.setParam("flakeSpread", flakeSpread);
    _ospMaterial.setParam("flakeJitter", flakeJitter);
    _ospMaterial.setParam("flakeRoughness", flakeRoughness);
}

void
HdOSPRayMaterial::UpdateLuminousMaterial()
{
    _ospMaterial.setParam(
           "color", vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    bool hasMetallicTex = false;
    bool hasRoughnessTex = false;
    bool hasOpacityTex = false;
    // set texture maps
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->diffuseColor) {
            name = "map_color";
        } else if (key == HdOSPRayMaterialTokens->transparency) {
            name = "transparency";
            hasOpacityTex = true;
        }

        if (name != "") {
            _ospMaterial.setParam(name.c_str(), value.ospTexture);
            if (value.hasXfm) {
                std::string st_translation = name + ".translation";
                std::string st_scale = name + ".scale";
                std::string st_rotation = name + ".rotation";
                _ospMaterial.setParam(st_translation.c_str(),
                                      vec2f(-value.xfm_translation[0],
                                            -value.xfm_translation[1]));
                _ospMaterial.setParam(st_scale.c_str(),
                                      vec2f(value.xfm_scale[0],
                                            value.xfm_scale[1]));
                _ospMaterial.setParam(st_rotation.c_str(), -value.xfm_rotation);
            }
        }
    }

    // set material params
    _ospMaterial.setParam("intensity", intensity);
    _ospMaterial.setParam("transparency",
                          (hasOpacityTex ? 1.0f : 1.0f - opacity));
}

void
HdOSPRayMaterial::UpdateGlassMaterial()
{
    //TODO: combine with thinglass
    _ospMaterial.setParam(
           "attenuationColor", vec3f(attenuationColor[0], attenuationColor[1], attenuationColor[2]));
    bool hasAttenuationColorTex = false;
    // set texture maps
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->attenuationColor) {
            name = "map_attenuationColor";
        }

        if (name != "") {
            _ospMaterial.setParam(name.c_str(), value.ospTexture);
            if (value.hasXfm) {
                std::string st_translation = name + ".translation";
                std::string st_scale = name + ".scale";
                std::string st_rotation = name + ".rotation";
                _ospMaterial.setParam(st_translation.c_str(),
                                      vec2f(-value.xfm_translation[0],
                                            -value.xfm_translation[1]));
                _ospMaterial.setParam(st_scale.c_str(),
                                      vec2f(value.xfm_scale[0],
                                            value.xfm_scale[1]));
                _ospMaterial.setParam(st_rotation.c_str(), -value.xfm_rotation);
            }
        }
    }

    // set material params
    _ospMaterial.setParam("eta", eta);
    _ospMaterial.setParam("attenuationDistance", attenuationDistance);
}

void
HdOSPRayMaterial::UpdateThinGlassMaterial()
{
    _ospMaterial.setParam(
           "attenuationColor", vec3f(attenuationColor[0], attenuationColor[1], attenuationColor[2]));
    bool hasAttenuationColorTex = false;
    // set texture maps
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->attenuationColor) {
            name = "map_attenuationColor";
        }

        if (name != "") {
            _ospMaterial.setParam(name.c_str(), value.ospTexture);
            if (value.hasXfm) {
                std::string st_translation = name + ".translation";
                std::string st_scale = name + ".scale";
                std::string st_rotation = name + ".rotation";
                _ospMaterial.setParam(st_translation.c_str(),
                                      vec2f(-value.xfm_translation[0],
                                            -value.xfm_translation[1]));
                _ospMaterial.setParam(st_scale.c_str(),
                                      vec2f(value.xfm_scale[0],
                                            value.xfm_scale[1]));
                _ospMaterial.setParam(st_rotation.c_str(), -value.xfm_rotation);
            }
        }
    }

    // set material params
    _ospMaterial.setParam("eta", eta);
    _ospMaterial.setParam("attenuationDistance", attenuationDistance);
    _ospMaterial.setParam("thickness", thickness);
}

void
HdOSPRayMaterial::UpdateSimpleMaterial(const std::string& rendererType)
{
    float avgFresnel = EvalAvgFresnel(ior);

    if (metallic == 0.f) {
        _ospMaterial.setParam(
               "kd",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2])
                      * (1.0f - avgFresnel));
        _ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2])
                      * avgFresnel);
        _ospMaterial.setParam("ns",
                              RoughnesToPhongExponent(std::sqrt(roughness)));
    } else {
        _ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        _ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2]));
        _ospMaterial.setParam("ns",
                              RoughnesToPhongExponent(std::sqrt(roughness)));
    }

    if (opacity < 1.0f) {
        float tf = 1.0f - opacity;
        _ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        _ospMaterial.setParam(
               "tf",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]) * tf);
    }
    if (_textures.find(HdOSPRayMaterialTokens->diffuseColor)
        != _textures.end()) {
        opp::Texture ospMapDiffuseTex
               = _textures[HdOSPRayMaterialTokens->diffuseColor].ospTexture;
        if (ospMapDiffuseTex) {
            _ospMaterial.setParam("map_kd", ospMapDiffuseTex);
            diffuseColor = GfVec3f(1.0);
        }
    }
}

void
HdOSPRayMaterial::UpdateScivisMaterial(const std::string& rendererType)
{
    _ospMaterial.setParam("ns", 10.f);
    _ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
    _ospMaterial.setParam(
           "kd", vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    if (_textures.find(HdOSPRayMaterialTokens->diffuseColor)
        != _textures.end()) {
        opp::Texture ospMapDiffuseTex
               = _textures[HdOSPRayMaterialTokens->diffuseColor].ospTexture;
        if (ospMapDiffuseTex) {
            _ospMaterial.setParam("map_kd", ospMapDiffuseTex);
            diffuseColor = GfVec3f(1.0);
        }
    }
    _ospMaterial.setParam("d", opacity);
}
