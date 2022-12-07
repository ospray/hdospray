// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "light.h"

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRayCylinderLight
///
/// The wrapper for mapping a USDLuxCylinderLight to an OSPRay cylinder light
/// source.
///
class HdOSPRayCylinderLight final : public HdOSPRayLight {
public:
    HdOSPRayCylinderLight(SdfPath const& id);
    ~HdOSPRayCylinderLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    float _radius { 1.f };
    float _length { 1.f };
};