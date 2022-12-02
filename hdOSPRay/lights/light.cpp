//
// Copyright 2020 Intel
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
#include "light.h"
#include "../config.h"

#include "../renderParam.h"
#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>

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

/* virtual */
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

    // Pull top-level OSPRay state out of the render param.
    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    // OSPRenderer renderer = ospRenderParam->GetOSPRayRenderer();

    // HdOSPRaySphereLight communicates to the scene graph and caches all
    // interesting values within this class. Later on Get() is called from
    // TaskState (RenderPass) to perform aggregation/pre-computation,
    // in order to make the shader execution efficient.

    // Change tracking
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

    _visibility = sceneDelegate->GetVisible(id);

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

    // generate the OSPLight source
    _PrepareOSPLight();

    // populates the light source to the OSPRay renderer
    _PopulateOSPLight(ospRenderParam);

    // TODO: implement handling of shadow collections

    *dirtyBits = Clean;
}

void
HdOSPRayLight::_PopulateOSPLight(HdOSPRayRenderParam* ospRenderParam) const
{
    // add the light source to the light list of the renderer
    ospRenderParam->AddHdOSPRayLight(GetId(), this);
}

/* virtual */
HdDirtyBits
HdOSPRayLight::GetInitialDirtyBitsMask() const
{
    // In the case of regular lights we want to sync all dirty bits, but
    // for area lights coming from the scenegraph we just want to extract
    // the Transform and Params for now.
    return AllDirty;
}