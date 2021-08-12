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
#ifndef HDOSPRAY_DISKLIGHT_H
#define HDOSPRAY_DISKLIGHT_H

#include "light.h"
PXR_NAMESPACE_OPEN_SCOPE

/// \class HdOSPRayDiskLight
///
/// The wrapper for mapping a USDLuxDiskLight to an OSPRay spot/disk light
/// source.
///
class HdOSPRayDiskLight final : public HdOSPRayLight {
public:
    HdOSPRayDiskLight(SdfPath const& id);
    ~HdOSPRayDiskLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    // radius of the disk light
    float _radius { 1.0f };

    float _openingAngle { 180.0f };

    // the texture file to model the emission
    // Note: not supported by OSPRay yet
    std::string _textureFile;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
