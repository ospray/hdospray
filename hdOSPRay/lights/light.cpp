// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "light.h"
#include "../config.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include "../renderParam.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usdLux/blackbody.h>

#include <iostream>

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayLightTokens,
    ((inputsIntensity,"inputs:intensity"))
    ((intensityQuantity,"inputs:ospray:intensityQuantity"))
    ((visibleToCamera,"ospray:visibleToCamera"))
    ((karmaVisibleToCamera,"karma:light:renderlightgeo"))
    ((prmanVisibleToCamera,"primvars:ri:attributes:visibility:camera"))
);

HdOSPRayLight::HdOSPRayLight(SdfPath const& id)
    : HdLight(id)
{
}

HdOSPRayLight::~HdOSPRayLight()
{
}

void
HdOSPRayLight::Finalize(HdRenderParam* renderParam)
{
    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);

    ospRenderParam->RemoveHdOSPRayLight(GetId());
}

void
HdOSPRayLight::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
                    HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    SdfPath const& id = GetId();

    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);

    HdDirtyBits bits = *dirtyBits;

    // Extrating the transformation/positioning of the light source in the scene
    if (bits & DirtyTransform) {
        #if HD_API_VERSION < 41
            VtValue transform = sceneDelegate->GetLightParamValue(
                id, HdTokens->transform);
            if (transform.IsHolding<GfMatrix4d>()) {
                _transform = transform.Get<GfMatrix4d>();
            } else {
                _transform = GfMatrix4d(1);
            }
        #else
            _transform = sceneDelegate->GetTransform(id);
        #endif
    }

    bool visible = sceneDelegate->GetVisible(id);
    bool visibilityDirty = (visible != _visibility);
    if (visibilityDirty)
        ospRenderParam->UpdateLightVersion();
    _visibility = visible;

    // Extract common Lighting Params
    if (bits & DirtyParams) {
        _emissionParam.color
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                        .Get<GfVec3f>();
        auto enableColorTemperatureVal = sceneDelegate->GetLightParamValue(
            id, HdLightTokens->enableColorTemperature);
        if (enableColorTemperatureVal.IsHolding<bool>())
            _emissionParam.enableColorTemperature = enableColorTemperatureVal.Get<bool>();
        auto colorTemperatureVal = sceneDelegate
            ->GetLightParamValue(id, HdLightTokens->colorTemperature);
        if (colorTemperatureVal.IsHolding<float>())
            _emissionParam.colorTemperature = colorTemperatureVal.Get<float>();
        auto diffuseVal = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
        if (diffuseVal.IsHolding<float>())
            _emissionParam.diffuse = diffuseVal.Get<float>();
        auto specularVal = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
        if (specularVal.IsHolding<float>())
            _emissionParam.specular = specularVal.Get<float>();
        auto exposureVal = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure);
        if (exposureVal.IsHolding<float>())
            _emissionParam.exposure = exposureVal.Get<float>();
        auto intensityVal = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity);
        if (intensityVal.IsHolding<float>())
            _emissionParam.intensity = intensityVal.Get<float>();
        // USD 22.08 not picking up inputs:intensity in HdLightTokens
        auto intensityVal2 = sceneDelegate->GetLightParamValue(id, HdOSPRayLightTokens->inputsIntensity);
        if (intensityVal2.IsHolding<float>())
            _emissionParam.intensity = intensityVal2.Get<float>();
        VtValue normalize = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
        if(normalize.IsHolding<int>())
        {
            _emissionParam.normalize = normalize.Get<int>() == 1;
        }
        else if (normalize.IsHolding<bool>()){
            _emissionParam.normalize = normalize.Get<bool>();
        }
        auto vtLightQuantity = sceneDelegate->GetLightParamValue(id, HdOSPRayLightTokens->intensityQuantity);
        if (vtLightQuantity.IsHolding<int>())
        {
            _emissionParam.intensityQuantity = (OSPIntensityQuantity)vtLightQuantity.Get<int>();
        }

        // checking if the light should be visible to the camera
        // since there is no USD arrtibute for this feature we are checking
        // the special primvars from Karma, Praman and OSPRay
        auto vtCameraVisibility = sceneDelegate->GetLightParamValue(id, HdOSPRayLightTokens->visibleToCamera);
        if (vtCameraVisibility.IsHolding<bool>())
        {
            _cameraVisibility = vtCameraVisibility.Get<bool>();
        }
        else if (vtCameraVisibility.IsHolding<int>())
        {
            _cameraVisibility = vtCameraVisibility.Get<int>();
        }
        vtCameraVisibility = sceneDelegate->GetLightParamValue(id, HdOSPRayLightTokens->karmaVisibleToCamera);
        if (vtCameraVisibility.IsHolding<bool>())
        {
            _cameraVisibility = vtCameraVisibility.Get<bool>();
        }
        else if (vtCameraVisibility.IsHolding<int>())
        {
            _cameraVisibility = vtCameraVisibility.Get<int>();
        }       
        vtCameraVisibility = sceneDelegate->GetLightParamValue(id, HdOSPRayLightTokens->prmanVisibleToCamera);
        if (vtCameraVisibility.IsHolding<bool>())
        {
            _cameraVisibility = vtCameraVisibility.Get<bool>();
        } else if (vtCameraVisibility.IsHolding<int>())
        {
            _cameraVisibility = vtCameraVisibility.Get<int>();
        }
    }

    if  (_emissionParam.enableColorTemperature)
    {
         _emissionParam.color = UsdLuxBlackbodyTemperatureAsRgb(_emissionParam.colorTemperature);

         // we apply a gama correction to match Houdini's Karama
         // TODO: doublecheck conversion algorithm to see if the output is SRGB or lin_SRGB
         _emissionParam.color[0] = pow(_emissionParam.color[0], 1.0f/2.2f);
         _emissionParam.color[1] = pow(_emissionParam.color[1], 1.0f/2.2f);
         _emissionParam.color[2] = pow(_emissionParam.color[2], 1.0f/2.2f);
    }

    // query light type specific parameters
    _LightSpecificSync(sceneDelegate, id, dirtyBits);

    if (bits & DirtyParams) {
        // generate the OSPLight source
        _PrepareOSPLight();

        // populates the light source to the OSPRay renderer
        _PopulateOSPLight(ospRenderParam);
    }

    // TODO: implement handling of shadow collections

    *dirtyBits = Clean;
}

void
HdOSPRayLight::_PopulateOSPLight(HdOSPRayRenderParam* ospRenderParam) const
{
    // add the light source to the light list of the renderer
    if (_ospLight)
        ospRenderParam->AddHdOSPRayLight(GetId(), this);
}

HdDirtyBits
HdOSPRayLight::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}