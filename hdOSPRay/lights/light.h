// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/pxr.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hd/version.h>

#include <vector>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayRenderParam;

/// \class HdOSPRayLight
///
///  Base class for implementing an OSPRay HdLight
///
class HdOSPRayLight : public HdLight {
public:
    HdOSPRayLight(SdfPath const& id);
    virtual ~HdOSPRayLight();

    virtual void Sync(HdSceneDelegate* sceneDelegate,
                      HdRenderParam* renderParam,
                      HdDirtyBits* dirtyBits) override;

    virtual HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Finalize(HdRenderParam* renderParam) override;

    inline bool IsVisible() const
    {
        return _visibility;
    }

    inline bool IsVisibleToCamera() const
    {
        return _cameraVisibility;
    }

    inline const opp::Light GetOSPLight() const
    {
        return _ospLight;
    }

private:
    void _PopulateOSPLight(HdOSPRayRenderParam* ospRenderParam) const;

protected:
    virtual void _LightSpecificSync(HdSceneDelegate* sceneDelegate,
                                    SdfPath const& id, HdDirtyBits* dirtyBits)
           = 0;

    virtual void _PrepareOSPLight() = 0;

    ///
    /// \struct EmissionParameter
    ///
    /// Contains the common parameters provided by every USD light source
    /// (USDLuxLight).
    ///
    struct EmissionParameter {
        GfVec3f color { 1.0f, 1.0f, 1.0f };
        bool enableColorTemperature { false };
        float colorTemperature { 6500.0f };
        // multiplier on diffuse interactions
        float diffuse { 1.0f };
        // multiplier on specular interactions
        float specular { 1.0f };
        // scales the power of the light source exponentially as a power of 2
        float exposure { 0.0f };
        // linear scale of the emission
        float intensity { 1.0f };
        // if the total emission (i.e., power) should not scale with the surface
        // area
        bool normalize { false };
        //  returns intensity multiplied by 2^exposure
        inline float ExposedIntensity()
        {
            return intensity * pow(2.0f, exposure);
        }
        // defines the meaning of the intensity * color value has in radiative
        // quantities (e.g., radiative intensity, radiance, power, ...)
        OSPIntensityQuantity intensityQuantity {
            OSPIntensityQuantity::OSP_INTENSITY_QUANTITY_UNKNOWN
        };
    };

    // USDLuxLight parameters to set up the emission of the light source
    EmissionParameter _emissionParam;
    // The transformation of the light source in the scene.
    GfMatrix4d _transform;

    // if light source is on (not to be confused with camera visible)
    bool _visibility { true };
    // if light source is visible to the camera
    bool _cameraVisibility { false };

    // reference to the equivalent OSPLight
    opp::Light _ospLight;
};