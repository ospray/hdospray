// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "renderDelegate.h"

#include "camera.h"
#include "config.h"
#include "instancer.h"
#include "renderBuffer.h"
#include "renderParam.h"
#include "renderPass.h"

#include <pxr/imaging/hd/resourceRegistry.h>

#include "basisCurves.h"
#include "lights/cylinderLight.h"
#include "lights/diskLight.h"
#include "lights/distantLight.h"
#include "lights/domeLight.h"
#include "lights/rectLight.h"
#include "lights/sphereLight.h"
#include "material.h"
#include "mesh.h"

// XXX: Add other Rprim types later
#include <pxr/imaging/hd/camera.h>
// XXX: Add other Sprim types later
#include <pxr/imaging/hd/bprim.h>
// XXX: Add bprim types
#include <pxr/imaging/hdSt/material.h>

#include <iostream>

TF_DEFINE_PUBLIC_TOKENS(HdOSPRayRenderSettingsTokens,
                        HDOSPRAY_RENDER_SETTINGS_TOKENS);

TF_DEFINE_PUBLIC_TOKENS(HdOSPRayTokens, HDOSPRAY_TOKENS);

const TfTokenVector HdOSPRayRenderDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->basisCurves,
};

const TfTokenVector HdOSPRayRenderDelegate::SUPPORTED_SPRIM_TYPES = {
    HdPrimTypeTokens->camera,       HdPrimTypeTokens->material,
    HdPrimTypeTokens->rectLight,    HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->sphereLight,  HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->distantLight, HdPrimTypeTokens->cylinderLight,
};

const TfTokenVector HdOSPRayRenderDelegate::SUPPORTED_BPRIM_TYPES = {
    HdPrimTypeTokens->renderBuffer,
};

std::mutex HdOSPRayRenderDelegate::_mutexResourceRegistry;
std::atomic_int HdOSPRayRenderDelegate::_counterResourceRegistry(0);
HdResourceRegistrySharedPtr HdOSPRayRenderDelegate::_resourceRegistry;

HdOSPRayRenderDelegate::HdOSPRayRenderDelegate()
    : HdRenderDelegate()
{
    _Initialize();
}

HdOSPRayRenderDelegate::HdOSPRayRenderDelegate(
       HdRenderSettingsMap const& settingsMap)
    : HdRenderDelegate(settingsMap)
{
    _Initialize();
}

void
HdOSPRayRenderDelegate::_Initialize()
{

#ifdef HDOSPRAY_PLUGIN_PTEX
    OSPError err = ospLoadModule("ptex");
    if (err != OSP_NO_ERROR)
        TF_RUNTIME_ERROR("hdosp::renderDelegate - error loading ptex");
    if (err != OSP_NO_ERROR)
        std::cout << "error loading ptex\n";
#endif

    if (HdOSPRayConfig::GetInstance().usePathTracing == 1)
        _renderer = opp::Renderer("pathtracer");
    else
        _renderer = opp::Renderer("scivis");

    _renderParam = std::make_shared<HdOSPRayRenderParam>(_renderer);

    std::lock_guard<std::mutex> guard(_mutexResourceRegistry);

    if (_counterResourceRegistry.fetch_add(1) == 0) {
        _resourceRegistry.reset(new HdResourceRegistry());
    }

    _settingDescriptors.push_back(
           { "Samples per frame", HdOSPRayRenderSettingsTokens->samplesPerFrame,
             VtValue(int(HdOSPRayConfig::GetInstance().samplesPerFrame)) });
    _settingDescriptors.push_back(
           { "Toggle denoiser", HdOSPRayRenderSettingsTokens->useDenoiser,
             VtValue(bool(HdOSPRayConfig::GetInstance().useDenoiser)) });
    _settingDescriptors.push_back(
           { "Pixelfilter type", HdOSPRayRenderSettingsTokens->pixelFilterType,
             VtValue(int(HdOSPRayConfig::GetInstance().pixelFilterType)) });
    _settingDescriptors.push_back(
           { "maxDepth", HdOSPRayRenderSettingsTokens->maxDepth,
             VtValue(int(HdOSPRayConfig::GetInstance().maxDepth)) });
    _settingDescriptors.push_back(
           { "rrStartDepth",
             HdOSPRayRenderSettingsTokens->russianRouletteStartDepth,
             VtValue(int(HdOSPRayConfig::GetInstance().rrStartDepth)) });
    _settingDescriptors.push_back(
           { "interactiveTargetFPS",
             HdOSPRayRenderSettingsTokens->interactiveTargetFPS,
             VtValue(float(
                    HdOSPRayConfig::GetInstance().interactiveTargetFPS)) });
    if (!HdOSPRayConfig::GetInstance().usePathTracing) {
        _settingDescriptors.push_back(
               { "Ambient occlusion samples",
                 HdOSPRayRenderSettingsTokens->ambientOcclusionSamples,
                 VtValue(int(HdOSPRayConfig::GetInstance()
                                    .ambientOcclusionSamples)) });
        _settingDescriptors.push_back(
               { "aoRadius", HdOSPRayRenderSettingsTokens->aoRadius,
                 VtValue(float(HdOSPRayConfig::GetInstance().aoRadius)) });
        _settingDescriptors.push_back(
               { "aoIntensity", HdOSPRayRenderSettingsTokens->aoIntensity,
                 VtValue(float(HdOSPRayConfig::GetInstance().aoIntensity)) });
    }
    _settingDescriptors.push_back(
           { "minContribution", HdOSPRayRenderSettingsTokens->minContribution,
             VtValue(float(HdOSPRayConfig::GetInstance().minContribution)) });
    _settingDescriptors.push_back(
           { "maxContribution", HdOSPRayRenderSettingsTokens->maxContribution,
             VtValue(float(HdOSPRayConfig::GetInstance().maxContribution)) });
    _settingDescriptors.push_back(
           { "samplesToConvergence",
             HdOSPRayRenderSettingsTokens->samplesToConvergence,
             VtValue(
                    int(HdOSPRayConfig::GetInstance().samplesToConvergence)) });
    _settingDescriptors.push_back(
           { "lightSamples", HdOSPRayRenderSettingsTokens->lightSamples,
             VtValue(int(HdOSPRayConfig::GetInstance().lightSamples)) });
    _settingDescriptors.push_back(
           { "ambientLight", HdOSPRayRenderSettingsTokens->ambientLight,
             VtValue(bool(HdOSPRayConfig::GetInstance().ambientLight)) });
    _settingDescriptors.push_back(
           { "staticDirectionalLights",
             HdOSPRayRenderSettingsTokens->staticDirectionalLights,
             VtValue(bool(
                    HdOSPRayConfig::GetInstance().staticDirectionalLights)) });
    _settingDescriptors.push_back(
           { "eyeLight", HdOSPRayRenderSettingsTokens->eyeLight,
             VtValue(bool(HdOSPRayConfig::GetInstance().eyeLight)) });
    _settingDescriptors.push_back(
           { "keyLight", HdOSPRayRenderSettingsTokens->keyLight,
             VtValue(bool(HdOSPRayConfig::GetInstance().keyLight)) });
    _settingDescriptors.push_back(
           { "fillLight", HdOSPRayRenderSettingsTokens->fillLight,
             VtValue(bool(HdOSPRayConfig::GetInstance().fillLight)) });
    _settingDescriptors.push_back(
           { "backLight", HdOSPRayRenderSettingsTokens->backLight,
             VtValue(bool(HdOSPRayConfig::GetInstance().backLight)) });
    _settingDescriptors.push_back(
           { "tmp_enabled", HdOSPRayRenderSettingsTokens->tmp_enabled,
             VtValue(bool(HdOSPRayConfig::GetInstance().tmp_enabled)) });
    _settingDescriptors.push_back(
           { "tmp_exposure", HdOSPRayRenderSettingsTokens->tmp_exposure,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_exposure)) });
    _settingDescriptors.push_back(
           { "tmp_contrast", HdOSPRayRenderSettingsTokens->tmp_contrast,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_contrast)) });
    _settingDescriptors.push_back(
           { "tmp_shoulder", HdOSPRayRenderSettingsTokens->tmp_shoulder,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_shoulder)) });
    _settingDescriptors.push_back(
           { "tmp_midIn", HdOSPRayRenderSettingsTokens->tmp_midIn,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_midIn)) });
    _settingDescriptors.push_back(
           { "tmp_midOut", HdOSPRayRenderSettingsTokens->tmp_midOut,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_midOut)) });
    _settingDescriptors.push_back(
           { "tmp_hdrMax", HdOSPRayRenderSettingsTokens->tmp_hdrMax,
             VtValue(float(HdOSPRayConfig::GetInstance().tmp_hdrMax)) });
    _settingDescriptors.push_back(
           { "tmp_acesColor", HdOSPRayRenderSettingsTokens->tmp_acesColor,
             VtValue(bool(HdOSPRayConfig::GetInstance().tmp_acesColor)) });
    _PopulateDefaultSettings(_settingDescriptors);
}

HdOSPRayRenderDelegate::~HdOSPRayRenderDelegate()
{
    std::lock_guard<std::mutex> guard(_mutexResourceRegistry);

    if (_counterResourceRegistry.fetch_sub(1) == 1) {
        _resourceRegistry.reset();
    }

    _renderParam.reset();
}

HdRenderParam*
HdOSPRayRenderDelegate::GetRenderParam() const
{
    return _renderParam.get();
}

void
HdOSPRayRenderDelegate::CommitResources(HdChangeTracker* tracker)
{
    auto& rp = _renderParam;
    const auto modelVersion = rp->GetModelVersion();
    if (modelVersion > _lastCommittedModelVersion) {
        _lastCommittedModelVersion = modelVersion;
    }
}

#if HD_API_VERSION < 41
TfToken
HdOSPRayRenderDelegate::GetMaterialNetworkSelector() const
{
    // Carson: this should be "HdOSPRayTokens->ospray", but we return glslfx so
    // that we work with many supplied shaders
    // TODO: is it possible to return both?
    return HdOSPRayTokens->glslfx;
}
#else
TfTokenVector
HdOSPRayRenderDelegate::GetMaterialRenderContexts() const
{
    // Carson: this should be "HdOSPRayTokens->ospray", but we return glslfx so
    // that we work with many supplied shaders
    // TODO: is it possible to return both?
    return {HdOSPRayTokens->glslfx, HdOSPRayTokens->osp};
}
#endif

TfTokenVector const&
HdOSPRayRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

TfTokenVector const&
HdOSPRayRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

TfTokenVector const&
HdOSPRayRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdResourceRegistrySharedPtr
HdOSPRayRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

HdAovDescriptor
HdOSPRayRenderDelegate::GetDefaultAovDescriptor(TfToken const& name) const
{
    if (name == HdAovTokens->color) {
        return HdAovDescriptor(HdFormatFloat32Vec4, true,
                               VtValue(GfVec4f(0.0f)));
    } else if (name == HdAovTokens->normal || name == HdAovTokens->Neye) {
        return HdAovDescriptor(HdFormatFloat32Vec3, false,
                               VtValue(GfVec3f(-1.0f)));
    } else if (name == HdAovTokens->depth) {
        return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
    } else if (name == HdAovTokens->primId || name == HdAovTokens->elementId
               || name == HdAovTokens->instanceId) {
        return HdAovDescriptor(HdFormatInt32, false, VtValue(0));
    } else {
        HdParsedAovToken aovId(name);
        if (aovId.isPrimvar) {
            return HdAovDescriptor(HdFormatFloat32Vec3, false,
                                   VtValue(GfVec3f(0.0f)));
        }
    }

    return HdAovDescriptor();
}

HdRenderPassSharedPtr
HdOSPRayRenderDelegate::CreateRenderPass(HdRenderIndex* index,
                                         HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(
           new HdOSPRayRenderPass(index, collection, _renderer, _renderParam));
}

#if HD_API_VERSION < 36
HdInstancer*
HdOSPRayRenderDelegate::CreateInstancer(HdSceneDelegate* delegate,
                                        SdfPath const& id,
                                        SdfPath const& instancerId)
{
    return new HdOSPRayInstancer(delegate, id, instancerId);
}
#else
HdInstancer*
HdOSPRayRenderDelegate::CreateInstancer(HdSceneDelegate* delegate,
                                        SdfPath const& id)
{
    return new HdOSPRayInstancer(delegate, id);
}
#endif

void
HdOSPRayRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
    delete instancer;
}

#if HD_API_VERSION < 36
HdRprim*
HdOSPRayRenderDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId,
                                    SdfPath const& instancerId)
{
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdOSPRayMesh(rprimId);
    } else if (typeId == HdPrimTypeTokens->basisCurves) {
        return new HdOSPRayBasisCurves(rprimId);
    } else {
        TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    }

    return nullptr;
}
#else
HdRprim*
HdOSPRayRenderDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId)
{
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdOSPRayMesh(rprimId);
    } else if (typeId == HdPrimTypeTokens->basisCurves) {
        return new HdOSPRayBasisCurves(rprimId);
    } else {
        TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    }

    return nullptr;
}
#endif

void
HdOSPRayRenderDelegate::DestroyRprim(HdRprim* rPrim)
{
    delete rPrim;
}

HdSprim*
HdOSPRayRenderDelegate::CreateSprim(TfToken const& typeId,
                                    SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdOSPRayCamera(sprimId);
    } else if (typeId == HdPrimTypeTokens->material) {
        return new HdOSPRayMaterial(sprimId);
    } else if (typeId == HdPrimTypeTokens->rectLight) {
        return new HdOSPRayRectLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->diskLight) {
        return new HdOSPRayDiskLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->sphereLight) {
        return new HdOSPRaySphereLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->domeLight) {
        return new HdOSPRayDomeLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->distantLight) {
        return new HdOSPRayDistantLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->cylinderLight) {
        return new HdOSPRayCylinderLight(sprimId);
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdSprim*
HdOSPRayRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    return CreateSprim(typeId, SdfPath::EmptyPath());
}

void
HdOSPRayRenderDelegate::DestroySprim(HdSprim* sPrim)
{
    delete sPrim;
}

HdBprim*
HdOSPRayRenderDelegate::CreateBprim(TfToken const& typeId,
                                    SdfPath const& bprimId)
{
    if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdOSPRayRenderBuffer(bprimId);
    } else {
        TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    }
    return nullptr;
}

HdBprim*
HdOSPRayRenderDelegate::CreateFallbackBprim(TfToken const& typeId)
{
    return CreateBprim(typeId, SdfPath::EmptyPath());
}

void
HdOSPRayRenderDelegate::DestroyBprim(HdBprim* bPrim)
{
    delete bPrim;
}

HdRenderSettingDescriptorList
HdOSPRayRenderDelegate::GetRenderSettingDescriptors() const
{
    return _settingDescriptors;
}
