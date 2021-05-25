//
// Copyright 2019 Intel
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
#include "sphereLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <iostream>

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(HdOSPRaySphereLightTokens, (treatAsPoint));

HdOSPRaySphereLight::HdOSPRaySphereLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRaySphereLight::~HdOSPRaySphereLight()
{
}

void
HdOSPRaySphereLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                        SdfPath const& id,
                                        HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        _radius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius)
                         .Get<float>();
        // TODO: find a way to query treatAsPoint
        _treatAsPoint
               = sceneDelegate
                        ->GetLightParamValue(
                               id, HdOSPRaySphereLightTokens->treatAsPoint)
                        .Get<bool>();
    }
}

void
HdOSPRaySphereLight::_PrepareOSPLight()
{

    float intensity = _emissionParam.ExposedIntensity();

    GfVec3f position(0, 0, 0);
    position = _transform.Transform(position);
    // Note: we are currently only considering translations
    // We could also consider scaling but is is not clear what to do
    // if the scaling i non-uniform

    _ospLight = opp::Light("sphere");
    _ospLight.setParam("position",
                       vec3f(position[0], position[1], position[2]));
    if (_treatAsPoint) {
        _ospLight.setParam("radius", 0.0f);
    } else {
        _ospLight.setParam("radius", _radius);
    }

    // emission
    if (_emissionParam.intensityQuantity
        != OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        _ospLight.setParam("intensityQuantity",
                           _emissionParam.intensityQuantity);
    }
    else
    {
        _ospLight.setParam("intensityQuantity",
                           OSP_INTENSITY_QUANTITY_RADIANCE);
    }

    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", intensity);
    _ospLight.setParam("visible", _visibility);
    _ospLight.commit();
}

PXR_NAMESPACE_CLOSE_SCOPE
