// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "distantLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/sdf/assetPath.h>

#include <iostream>

using namespace rkcommon::math;

HdOSPRayDistantLight::HdOSPRayDistantLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRayDistantLight::~HdOSPRayDistantLight()
{
}

void
HdOSPRayDistantLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                         SdfPath const& id,
                                         HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        _angle = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle)
                        .Get<float>();
    }
}

void
HdOSPRayDistantLight::_PrepareOSPLight()
{
    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_IRRADIANCE
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }
    GfVec3f direction(0, 0, -1);

    // only apply the rotational part of the transformation
    GfMatrix3d rotTransform = _transform.ExtractRotationMatrix();
    direction = direction * rotTransform;
    if (std::abs(direction.GetLength() - 1.0f) > GF_MIN_VECTOR_LENGTH) {
        direction.Normalize();
    }

    _ospLight = opp::Light("distant");
    // placement
    _ospLight.setParam("direction",
                       vec3f(direction[0], direction[1], direction[2]));
    _ospLight.setParam("angularDiameter", _angle);
    // emission
    _ospLight.setParam("intensityQuantity", intensityQuantity);
    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", _emissionParam.ExposedIntensity());
    _ospLight.setParam("visible", _cameraVisibility);
    _ospLight.commit();
}