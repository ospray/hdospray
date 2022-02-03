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
#include "cylinderLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/sdf/assetPath.h>

#include <iostream>

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

HdOSPRayCylinderLight::HdOSPRayCylinderLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRayCylinderLight::~HdOSPRayCylinderLight()
{
}

void
HdOSPRayCylinderLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                          SdfPath const& id,
                                          HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        _radius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius)
                         .Get<float>();
        auto length
               = sceneDelegate->GetLightParamValue(id, HdLightTokens->length);
        if (length.IsHolding<float>()) {
            _length = length.Get<float>();
        }
    }
}

void
HdOSPRayCylinderLight::_PrepareOSPLight()
{
    float intensity = _emissionParam.ExposedIntensity();

    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_POWER
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }

    GfVec3f position0(-_length/2.0f, 0, 0);
    GfVec3f position1(_length/2.0f, 0, 0);
    position0 = _transform.Transform(position0);
    position1 = _transform.Transform(position1);

    _ospLight = opp::Light("cylinder");
    // placement
    _ospLight.setParam("position0",
                       vec3f(position0[0], position0[1], position0[2]));
    _ospLight.setParam("position1",
                       vec3f(position1[0], position1[1], position1[2]));
    _ospLight.setParam("radius", _radius);

    // emission
    _ospLight.setParam("intensityQuantity", intensityQuantity);

    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", intensity);
    _ospLight.setParam("visible", _cameraVisibility);
    _ospLight.commit();
}

PXR_NAMESPACE_CLOSE_SCOPE
