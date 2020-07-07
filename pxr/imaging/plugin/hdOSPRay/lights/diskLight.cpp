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
#include "pxr/imaging/hdOSPRay/lights/diskLight.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include "pxr/base/gf/matrix4d.h"

#include "pxr/usd/sdf/assetPath.h"

#include "ospray/ospray_util.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

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
    }
}

void
HdOSPRayDiskLight::_PrepareOSPLight()
{
    float intensity = 1.0f;
    if (_emissionParam.exposure != 0.0f) {
        intensity = pow(2.0f, _emissionParam.exposure);
    } else {
        intensity = _emissionParam.intensity;
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
    // checks if we are dealing with a disk or spot light
    if (radius > 0.0) {
        // in case of a disk light intensity represents the emitted raidnace
        // and has to be converted to the equivalent of intensity for a spot
        // light
        float power = (M_PI * radius * radius) * intensity * M_PI;
        intensity = power / (2.0f * M_PI);
        // scaling factor to correct for a legacy factor on OSPRay
        intensity *= 2.0;
    }

    // in OSPRay a disk light is represented by a spot light
    // having a radius > 0.0
    _ospLight = ospNewLight("spot");
    // placement
    ospSetVec3f(_ospLight, "position", position[0], position[1], position[2]);
    ospSetVec3f(_ospLight, "direction", direction[0], direction[1],
                direction[2]);
    ospSetFloat(_ospLight, "radius", radius);
    ospSetFloat(_ospLight, "openingAngle", 180.0f);
    ospSetFloat(_ospLight, "penumbraAngle", 0.0f);

    // emission
    ospSetVec3f(_ospLight, "color", _emissionParam.color[0],
                _emissionParam.color[1], _emissionParam.color[2]);
    ospSetFloat(_ospLight, "intensity", intensity);
    ospSetBool(_ospLight, "visible", _visibility);
    ospCommit(_ospLight);
}

PXR_NAMESPACE_CLOSE_SCOPE
