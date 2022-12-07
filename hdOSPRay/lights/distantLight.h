// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "light.h"

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRayDistantLight
///
/// The wrapper for mapping a USDLuxDistantLight to an OSPRay distant light
/// source.
///
class HdOSPRayDistantLight final : public HdOSPRayLight {
public:
    HdOSPRayDistantLight(SdfPath const& id);
    ~HdOSPRayDistantLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    // diameter in angle of the distant light
    // 0.53 degrees is equivalent to the diameter of the sun
    float _angle { 0.53f };
};