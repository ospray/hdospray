// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "rectLight.h"

#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/rotation.h>

#include <pxr/usd/sdf/assetPath.h>

#include <iostream>

using namespace rkcommon::math;

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayRectLightTokens,
    ((edge1,"ospray:edge1"))
    ((edge2,"ospray:edge2"))
    ((position,"ospray:position"))
);

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
        auto edge1Param = sceneDelegate->GetLightParamValue(id, HdOSPRayRectLightTokens->edge1);
        if (edge1Param.IsHolding<GfVec3f>()) {
            _edge1 = edge1Param.Get<GfVec3f>();
        }
        auto edge2Param = sceneDelegate->GetLightParamValue(id, HdOSPRayRectLightTokens->edge2);
        if (edge2Param.IsHolding<GfVec3f>()) {
            _edge2 = edge2Param.Get<GfVec3f>();
        }
        auto positionParam = sceneDelegate->GetLightParamValue(id, HdOSPRayRectLightTokens->position);
        if (positionParam.IsHolding<GfVec3f>()) {
            _position = positionParam.Get<GfVec3f>();
            _positionSet = true;
        }
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
    GfVec3f xAxis(1.f, 0.f, 0.f);
    GfVec3f yAxis(0.f, -1.f, 0.f);

    // caluculating the OSPRay quad parametrization from the transformed
    // corner points
    GfVec3f osp_edge1 = xAxis * _width;
    GfVec3f osp_edge2 = yAxis * _height;
    GfVec3f osp_position = GfVec3f(usd_rectLightCenter - xAxis * _width/2.f - yAxis * _height/2.f);

    // the quad light emits light in the direction
    // defined by the cross product of edge1 and edge2
    GfVec3f osp_nedge1 = osp_edge1 / osp_edge1.GetLength();
    GfVec3f osp_nedge2 = osp_edge2 / osp_edge2.GetLength();
    GfVec3f osp_quadDir = GfCross(osp_nedge1, osp_nedge2);

    // checking, if we need to flip the direction of the osp quad light
    // to match the direction of the USD rect light
    if (GfDot(osp_quadDir, usd_rectLightDir) < 0.0f) {
        osp_position = osp_position + osp_edge2;
        osp_edge2 = osp_edge2 * -1.0f;
    }

    OSPIntensityQuantity intensityQuantity = _emissionParam.intensityQuantity;
    if (intensityQuantity
        == OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        intensityQuantity = _emissionParam.normalize
               ? OSP_INTENSITY_QUANTITY_POWER
               : OSP_INTENSITY_QUANTITY_RADIANCE;
    }

    if (_edge1 != GfVec3f(0.f))
        osp_edge1 = _edge1;
    if (_edge2 != GfVec3f(0.f))
        osp_edge2 = _edge2;
    if (_positionSet)
        osp_position = _position;

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