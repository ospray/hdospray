// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../texture.h"
#include "light.h"

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRayDomeLight
///
/// The wrapper for mapping a USDLuxDomeLight to an OSPRay hdri light source.
///
class HdOSPRayDomeLight final : public HdOSPRayLight {
public:
    HdOSPRayDomeLight(SdfPath const& id);
    ~HdOSPRayDomeLight();

protected:
    void _LightSpecificSync(HdSceneDelegate* sceneDelegate, SdfPath const& id,
                            HdDirtyBits* dirtyBits) override;

    void _PrepareOSPLight() override;

private:
    // the loaded texture
    HdOSPRayTexture _hdriTexture;
    // path to the lat/long texture file
    std::string _textureFile;
};