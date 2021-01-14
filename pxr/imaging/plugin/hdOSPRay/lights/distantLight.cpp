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
#include "pxr/imaging/hdOSPRay/lights/distantLight.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include "pxr/base/gf/matrix3d.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/usd/sdf/assetPath.h"

#include <iostream>

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

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
    GfVec3f direction(0, 0, -1);

    // only apply the rotational part of the transformation
    GfMatrix3d rotTransform = _transform.ExtractRotationMatrix();
    direction = direction * rotTransform;
    if (std::abs(direction.GetLength() - 1.0f) > GF_MIN_VECTOR_LENGTH) {
        std::cout << "DistantLight: direction is not normalized" << std::endl;
        direction.Normalize();
    }

    _ospLight = opp::Light("distant");
    // placement
    _ospLight.setParam("direction",
                       vec3f(direction[0], direction[1], direction[2]));
    _ospLight.setParam("angularDiameter", _angle);
    // emission
    if (_emissionParam.intensityQuantity
        != OSPIntensityQuantiy::OSP_INTENSITY_QUANTITY_UNKNOWN) {
        _ospLight.setParam("intensityQuantity",
                           _emissionParam.intensityQuantity);
    }
    _ospLight.setParam("color",
                       vec3f(_emissionParam.color[0], _emissionParam.color[1],
                             _emissionParam.color[2]));
    _ospLight.setParam("intensity", _emissionParam.ExposedIntensity());
    _ospLight.setParam("visible", _visibility);
    _ospLight.commit();
}

PXR_NAMESPACE_CLOSE_SCOPE
