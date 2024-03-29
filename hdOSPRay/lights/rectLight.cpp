// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "rectLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/sdf/assetPath.h>

#include <iostream>

using namespace rkcommon::math;

HdOSPRayRectLight::HdOSPRayRectLight(SdfPath const& id)
    : HdOSPRayLight(id)
{
}

HdOSPRayRectLight::~HdOSPRayRectLight()
{
}

void
HdOSPRayRectLight::_LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                      SdfPath const& id, HdDirtyBits* dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;
    if (bits & DirtyParams) {
        _width = sceneDelegate->GetLightParamValue(id, HdLightTokens->width)
                        .Get<float>();
        _height = sceneDelegate->GetLightParamValue(id, HdLightTokens->height)
                         .Get<float>();
        auto val = sceneDelegate->GetLightParamValue(
               id, HdLightTokens->textureFile);
        if (!val.IsEmpty()) {
            SdfAssetPath textureFilePath = val.UncheckedGet<SdfAssetPath>();
            _textureFile = textureFilePath.GetResolvedPath();
        }
    }
}

void
HdOSPRayRectLight::_PrepareOSPLight()
{
    // USD RectLight: we need to evaluate the orientation
    // of the RectLight after the transformation
    // Note: USD defines the rect light on the XY-plane and its emitts light
    // into the -Z direction
    GfVec3f usd_rectLightCenter(0, 0, 0);
    GfVec3f usd_rectLightDir(0, 0, -1);

    usd_rectLightCenter = _transform.Transform(usd_rectLightCenter);
    usd_rectLightDir
           = _transform.Transform(usd_rectLightDir) - usd_rectLightCenter;
    usd_rectLightDir /= usd_rectLightDir.GetLength();

    // OSPRay quad light
    // estimation of the parametrization of the OSPray quad light

    // the corner points of the quad light
    GfVec3f ops_quadUpperLeft(-_width / 2.0f, _height / 2.0f, 0);
    GfVec3f osp_quadLowerLeft(-_width / 2.0f, -_height / 2.0f, 0);
    GfVec3f osp_quadLowerRight(_width / 2.0f, -_height / 2.0f, 0);

    ops_quadUpperLeft = _transform.Transform(ops_quadUpperLeft);
    osp_quadLowerLeft = _transform.Transform(osp_quadLowerLeft);
    osp_quadLowerRight = _transform.Transform(osp_quadLowerRight);

    // caluculating the OSPRay quad parametrization from the transformed
    // corner points
    GfVec3f osp_position = GfVec3f(osp_quadLowerLeft[0], osp_quadLowerLeft[1],
                                   osp_quadLowerLeft[2]);
    GfVec3f osp_edge1 = GfVec3f(osp_quadLowerRight[0] - osp_position[0],
                                osp_quadLowerRight[1] - osp_position[1],
                                osp_quadLowerRight[2] - osp_position[2]);
    GfVec3f osp_edge2 = GfVec3f(ops_quadUpperLeft[0] - osp_position[0],
                                ops_quadUpperLeft[1] - osp_position[1],
                                ops_quadUpperLeft[2] - osp_position[2]);

    // the quad light emits light in the direction
    // defined by the cross product of edge1 and edge2
    GfVec3f osp_nedge1 = osp_edge1 / osp_edge1.GetLength();
    GfVec3f osp_nedge2 = osp_edge2 / osp_edge2.GetLength();
    GfVec3f osp_quadDir = GfCross(osp_nedge1, osp_nedge2);

    // checking, if we need to flip the direction of the osp quad light
    // to match the direction of the USD rect light
    if (GfDot(osp_quadDir, usd_rectLightDir) < 0.0f) {
        osp_position = osp_position + osp_edge1;
        osp_edge1 = osp_edge1 * -1.0f;
    }

    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_POWER
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }

    _ospLight = opp::Light("quad");
    // placement
    _ospLight.setParam(
           "position",
           vec3f(osp_position[0], osp_position[1], osp_position[2]));
    _ospLight.setParam("edge1",
                       vec3f(osp_edge1[0], osp_edge1[1], osp_edge1[2]));
    _ospLight.setParam("edge2",
                       vec3f(osp_edge2[0], osp_edge2[1], osp_edge2[2]));
    // emission
    _ospLight.setParam("intensityQuantity", intensityQuantity);
    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", _emissionParam.ExposedIntensity());
    _ospLight.setParam("visible", _cameraVisibility);
    _ospLight.commit();
}