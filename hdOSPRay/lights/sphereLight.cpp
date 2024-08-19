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

        {
            const VtValue tValue {
                sceneDelegate
                    ->GetLightParamValue(
                        id, HdLightTokens->shapingConeAngle) };

            if (tValue.IsHolding<float>())
                _coneAngle = tValue.Get<float>();
            else
                _coneAngle.reset();
        }
        _coneSoftness =
            sceneDelegate
                ->GetLightParamValue(id, HdLightTokens->shapingConeSoftness)
                    .GetWithDefault<float>(0.f);
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

    if (_coneAngle) {
        // We need to have a "spot" light
        if (!_ospLight || !_lightIsConeLight)
            _ospLight = opp::Light("spot");
        _lightIsConeLight = true;

        _ospLight.setParam("openingAngle", *_coneAngle);

        // describes "cutoff softness for cone angle"
        _ospLight.setParam("penumbraAngle", _coneSoftness);

        GfVec3f direction { 0.f, 0.f, -1.f };
        direction = _transform.TransformDir(direction);
        // Normalized direction required to be accurate, otherwise value will
        // not be accepted
        direction.Normalize();
        _ospLight.setParam("direction",
                           vec3f { direction[0], direction[1], direction[2] });
    }
    else {
        // We need to have a "sphere" light
        if (!_ospLight || _lightIsConeLight)
            _ospLight = opp::Light("sphere");
        _lightIsConeLight = false;
    }

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