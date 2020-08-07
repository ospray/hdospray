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
#include "pxr/imaging/hdOSPRay/lights/light.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hdOSPRay/renderParam.h"

#include "pxr/base/gf/matrix4d.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

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
        VtValue transform = sceneDelegate->GetLightParamValue(
               id, HdLightTokens->transform);
        if (transform.IsHolding<GfMatrix4d>()) {
            _transform = transform.Get<GfMatrix4d>();
        } else {
            _transform = GfMatrix4d(1);
        }
    }

    // Extract common Lighting Params
    if (bits & DirtyParams) {
        _emissionParam.color
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                        .Get<GfVec3f>();
        _emissionParam.enableColorTemperature
               = sceneDelegate
                        ->GetLightParamValue(
                               id, HdLightTokens->enableColorTemperature)
                        .Get<bool>();
        _emissionParam.colorTemperature
               = sceneDelegate
                        ->GetLightParamValue(id,
                                             HdLightTokens->colorTemperature)
                        .Get<float>();
        _emissionParam.diffuse
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                        .Get<float>();
        _emissionParam.specular
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular)
                        .Get<float>();
        _emissionParam.exposure
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure)
                        .Get<float>();
        _emissionParam.intensity
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity)
                        .Get<float>();
        _emissionParam.normalize
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize)
                        .Get<bool>();
        _visibility = sceneDelegate->GetVisible(id);
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

PXR_NAMESPACE_CLOSE_SCOPE
