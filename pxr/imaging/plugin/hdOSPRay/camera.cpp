// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "camera.h"
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(HdOSPRayCameraTokens,
                         (horizontalAperture)(verticalAperture)(focalLength));

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

    auto horizontalAperture = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->horizontalAperture);
    auto verticalAperture = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->verticalAperture);
    auto focalLength = sceneDelegate->GetCameraParamValue(
           GetId(), HdOSPRayCameraTokens->focalLength);

    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    OSPRayCameraParams params = ospRenderParam->GetCameraParams();

    if (horizontalAperture.IsHolding<float>()) {
        params.horizontalAperture = horizontalAperture.Get<float>();
    }
    if (verticalAperture.IsHolding<float>()) {
        params.verticalAperture = verticalAperture.Get<float>();
    }
    if (focalLength.IsHolding<float>()) {
        params.focalLength = focalLength.Get<float>();
    }

    ospRenderParam->SetCameraParams(params);
}

PXR_NAMESPACE_CLOSE_SCOPE
