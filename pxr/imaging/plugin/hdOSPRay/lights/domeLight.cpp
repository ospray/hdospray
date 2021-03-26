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
#include "pxr/imaging/hdOSPRay/lights/domeLight.h"
#include "pxr/imaging/hdOSPRay/texture.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include "pxr/base/gf/matrix4d.h"

#include "pxr/usd/sdf/assetPath.h"

#include "rkcommon/math/vec.h"

#include <iostream>

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

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
        SdfAssetPath textureFilePath
               = sceneDelegate
                        ->GetLightParamValue(id, HdLightTokens->textureFile)
                        .Get<SdfAssetPath>();
        _textureFile = textureFilePath.GetResolvedPath();
    }
    _cameraVisibility = true; // override light visibility for dome lights
}

void
HdOSPRayDomeLight::_PrepareOSPLight()
{
    GfVec3f upDirection(0.0, 1.0, 0.0);
    GfVec3f centerDirection(0.0, 0.0, 1.0);

    upDirection = _transform.Transform(upDirection);
    centerDirection = _transform.Transform(centerDirection);

    float intensity = _emissionParam.intensity;
    if (_emissionParam.exposure != 0.0f)
        intensity *= pow(2.0f, _emissionParam.exposure);
    // intensity = pow(intensity, 1.f/2.2f);

    _hdriTexture = LoadOIIOTexture2D(_textureFile);

    _ospLight = opp::Light("hdri");
    // placement
    _ospLight.setParam("up",
                       vec3f(upDirection[0], upDirection[1], upDirection[2]));
    _ospLight.setParam(
           "direction",
           vec3f(centerDirection[0], centerDirection[1], centerDirection[2]));
    // emission
    _ospLight.setParam("map", _hdriTexture);
    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", intensity);
    _ospLight.setParam("visible", _cameraVisibility);
    _ospLight.commit();
}

PXR_NAMESPACE_CLOSE_SCOPE
