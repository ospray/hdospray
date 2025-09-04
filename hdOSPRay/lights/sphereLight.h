// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "light.h"

#include <optional>

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRaySphereLight
///
/// The wrapper class for mapping a USDLuxSphereLight to an OSPRay point/sphere
/// light source.
///
class HdOSPRaySphereLight final : public HdOSPRayLight {
public:
    HdOSPRaySphereLight(SdfPath const& id);
    ~HdOSPRaySphereLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    // the radius of the sphere
    float _radius { 0.5f };
    // if the sphere light should be treated as a virtual point light
    bool _treatAsPoint { false };
    // A Sphere Light can optionally be shaped into a cone
    std::optional<float> _coneAngle;
    // The cone can further be refined, by having a soft outer ring
    float _coneSoftness { 0.f };
    // To know when to recreate the light
    bool _lightIsConeLight { false };
};