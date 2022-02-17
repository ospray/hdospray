// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "camera.h"
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

HdOSPRayCamera::HdOSPRayCamera(SdfPath const& id)
    : HdCamera(id)
{
}

HdOSPRayCamera::~HdOSPRayCamera() = default;

void
HdOSPRayCamera::Sync(HdSceneDelegate* sceneDelegate,
                     HdRenderParam* _renderParam, HdDirtyBits* dirtyBits)
{
    HdCamera::Sync(sceneDelegate, _renderParam, dirtyBits);

    auto fStop = sceneDelegate->GetCameraParamValue(
           GetId(), HdCameraTokens->fStop);
    auto horizontalAperture = sceneDelegate->GetCameraParamValue(
           GetId(), HdCameraTokens->horizontalAperture);
    auto verticalAperture = sceneDelegate->GetCameraParamValue(
           GetId(), HdCameraTokens->verticalAperture);
    auto focusDistance = sceneDelegate->GetCameraParamValue(
           GetId(), HdCameraTokens->focusDistance);
    auto focalLength = sceneDelegate->GetCameraParamValue(
           GetId(), HdCameraTokens->focalLength);

    if (fStop.IsHolding<float>())
        _fStop = fStop.Get<float>();
    if (horizontalAperture.IsHolding<float>())
        _horizontalAperture = horizontalAperture.Get<float>();
    if (verticalAperture.IsHolding<float>())
        _verticalAperture = verticalAperture.Get<float>();
    if (focalLength.IsHolding<float>())
        _focalLength = focalLength.Get<float>();
    if (focusDistance.IsHolding<float>())
        _focusDistance = focusDistance.Get<float>();

}

PXR_NAMESPACE_CLOSE_SCOPE
