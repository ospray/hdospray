// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "diskLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/sdf/assetPath.h>

#include <iostream>

using namespace rkcommon::math;

HdOSPRayDiskLight::HdOSPRayDiskLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRayDiskLight::~HdOSPRayDiskLight()
{
}

void
HdOSPRayDiskLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                      SdfPath const& id, HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        _radius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius)
                         .Get<float>();
        auto vtOpeningAngle = sceneDelegate->GetLightParamValue(
               id, HdLightTokens->shapingConeAngle);
        if (vtOpeningAngle.IsHolding<float>()) {
            _openingAngle = vtOpeningAngle.Get<float>();
        }
    }
}

void
HdOSPRayDiskLight::_PrepareOSPLight()
{
    float intensity = _emissionParam.ExposedIntensity();

    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_POWER
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }

    // the initial center of the disk
    GfVec3f position(0, 0, 0);
    // positing to determine the direction of the spotlight
    GfVec3f position1(0, 0, -1);
    // position to dertermine the scale of the original
    // radius due to transformations
    GfVec3f position2(1, 0, 0);

    // transforming the disk light to its location
    // in the scene (including translation and scaling)
    position = _transform.Transform(position);
    position1 = _transform.Transform(position1);
    position2 = _transform.Transform(position2);

    float radiusScale = (position2 - position).GetLength();

    GfVec3f direction = position1 - position;
    direction.Normalize();

    float radius = _radius * radiusScale;

    // in OSPRay a disk light is represented by a spot light
    // having a radius > 0.0
    _ospLight = opp::Light("spot");
    // placement
    _ospLight.setParam("position",
                       vec3f(position[0], position[1], position[2]));
    _ospLight.setParam("direction",
                       vec3f(direction[0], direction[1], direction[2]));
    _ospLight.setParam("radius", radius);

    _ospLight.setParam("openingAngle", _openingAngle);
    _ospLight.setParam("penumbraAngle", 0.0f);

    // emission
    _ospLight.setParam("intensityQuantity", intensityQuantity);

    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", intensity);
    _ospLight.setParam("visible", _cameraVisibility);
    _ospLight.commit();
}