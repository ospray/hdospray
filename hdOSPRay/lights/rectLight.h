// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "light.h"

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRayRectLight
///
/// The wrapper class for mapping a USDLuxRectLight to an OSPRay quad light
/// source.
///
class HdOSPRayRectLight final : public HdOSPRayLight {
public:
    HdOSPRayRectLight(SdfPath const& id);
    ~HdOSPRayRectLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    /// width and height of the USDRectLight
    float _width { 1.0f };
    float _height { 1.0f };

    /// the texture file to model the emission
    /// Note: not supported by OSPRay yet
    std::string _textureFile;
};