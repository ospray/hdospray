// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "domeLight.h"
#include "../texture.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/sdf/assetPath.h>

#include <rkcommon/math/vec.h>

#include <iostream>

using namespace rkcommon::math;

HdOSPRayDomeLight::HdOSPRayDomeLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRayDomeLight::~HdOSPRayDomeLight()
{
}

void
HdOSPRayDomeLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                      SdfPath const& id, HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        VtValue v = sceneDelegate->GetLightParamValue(
               id, HdLightTokens->textureFile);
        if (v.IsHolding<SdfAssetPath>()) {
            _textureFile = v.UncheckedGet<SdfAssetPath>().GetResolvedPath();
        }
    }
}

void
HdOSPRayDomeLight::_PrepareOSPLight()
{
    GfVec3f upDirection(0.0, 1.0, 0.0);
    GfVec3f centerDirection(0.0, 0.0, 1.0);

    upDirection = _transform.Transform(upDirection);
    centerDirection = _transform.Transform(centerDirection);

    const auto& result = LoadOIIOTexture2D(_textureFile);
    _hdriTexture = result.first;

    if (_hdriTexture) {
        _ospLight = opp::Light("hdri");
        // placement
        _ospLight.setParam(
               "up", vec3f(upDirection[0], upDirection[1], upDirection[2]));
        _ospLight.setParam("direction",
                           vec3f(centerDirection[0], centerDirection[1],
                                 centerDirection[2]));
        // emission
        _ospLight.setParam("map", _hdriTexture);
        _ospLight.setParam("color",
                           vec3f(_emissionParam.color[0],
                                 _emissionParam.color[1],
                                 _emissionParam.color[2]));
        _ospLight.setParam("intensity", _emissionParam.ExposedIntensity());
        _ospLight.setParam("visible", _cameraVisibility);
        _ospLight.commit();
    } else {
        _ospLight = opp::Light("ambient");
        _ospLight.setParam("color",
                           vec3f(_emissionParam.color[0],
                                 _emissionParam.color[1],
                                 _emissionParam.color[2]));
        _ospLight.setParam("intensity", _emissionParam.ExposedIntensity());
        _ospLight.commit();
    }
}