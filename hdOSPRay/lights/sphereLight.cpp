// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "sphereLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <iostream>

using namespace rkcommon::math;

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

    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_POWER
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }

    GfVec3f position(0, 0, 0);
    position = _transform.Transform(position);
    // Note: we are currently only considering translations
    // We could also consider scaling but is is not clear what to do
    // if the scaling i non-uniform

    _ospLight = opp::Light("sphere");
    _ospLight.setParam("position",
                       vec3f(position[0], position[1], position[2]));
    if (_treatAsPoint)
        _ospLight.setParam("radius", 0.0f);
    else
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