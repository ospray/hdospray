// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "renderParam.h"
#include <memory>
#include <pxr/imaging/hd/camera.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayCamera final : public HdCamera {
public:
    HdOSPRayCamera(SdfPath const& id);
    ~HdOSPRayCamera();

    void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override final;

    float GetFStop() const
    {
        return _fStop;
    }
    float GetFocalLength() const
    {
        return _focalLength;
    }
    float GetFocusDistance() const
    {
        return _focusDistance;
    }
    float GetHorizontalAperture() const
    {
        return _horizontalAperture;
    }
    float GetVerticalAperture() const
    {
        return _verticalAperture;
    }

protected:
    float _fStop { 0.f };
    float _horizontalAperture { 20.955f };
    float _verticalAperture { 15.2908f };
    float _focalLength { 0.f };
    float _focusDistance { 0.f };
};