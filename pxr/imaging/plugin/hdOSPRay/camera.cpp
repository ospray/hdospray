// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "camera.h"
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(HdOSPRayCameraTokens,
                         (fStop)(focalLength)(focusDistance));

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
    auto renderParam = dynamic_cast<HdOSPRayRenderParam*>(_renderParam);

    auto fStop = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->fStop);
    auto focusDistance = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->focusDistance);
    auto focalLength = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->focalLength);

    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    OSPRayCameraParams params = ospRenderParam->GetCameraParams();

    if (fStop.IsHolding<float>()) {
        params.aperture = fStop.Get<float>();
    }
    if (focalLength.IsHolding<float>()) {
        params.focusDistance = focalLength.Get<float>()*10.f;
    }
    if (focusDistance.IsHolding<float>()) {
        params.focusDistance = focusDistance.Get<float>();
    }

    ospRenderParam->SetCameraParams(params);
}

PXR_NAMESPACE_CLOSE_SCOPE
