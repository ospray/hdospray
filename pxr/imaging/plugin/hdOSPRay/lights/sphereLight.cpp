//
// Copyright 2019 Intel
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
#include "pxr/imaging/hdOSPRay/lights/sphereLight.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include "pxr/base/gf/matrix4d.h"

#include "ospray/ospray_util.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

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

    float intensity = 1.0f;
    if (_emissionParam.exposure != 0.0f) {
        intensity = pow(2.0f, _emissionParam.exposure);
    } else {
        intensity = _emissionParam.intensity;
    }

    GfVec3f position(0, 0, 0);
    position = _transform.Transform(position);
    // Note: we are currently only considering translations
    // We could also consider scaling but is is not clear what to do
    // if the scaling i non-uniform

    _ospLight = ospNewLight("sphere");
    ospSetVec3f(_ospLight, "position", position[0], position[1], position[2]);
    if (_treatAsPoint) {
        ospSetFloat(_ospLight, "radius", 0.0f);
    } else {
        ospSetFloat(_ospLight, "radius", _radius);
        // in case of a disk light intensity represents the emitted raidnace
        // and has to be converted to the equivalent of intensity for a point
        // light
        float power = (4.0f * M_PI * _radius * _radius) * intensity * M_PI;
        intensity = power / (4.0f * M_PI);
    }

    // emission
    ospSetVec3f(_ospLight, "color", _emissionParam.color[0],
                _emissionParam.color[1], _emissionParam.color[2]);
    ospSetFloat(_ospLight, "intensity", intensity);
    ospSetBool(_ospLight, "visible", _visibility);
    ospCommit(_ospLight);
}

PXR_NAMESPACE_CLOSE_SCOPE