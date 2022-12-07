// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "light.h"

PXR_NAMESPACE_USING_DIRECTIVE

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