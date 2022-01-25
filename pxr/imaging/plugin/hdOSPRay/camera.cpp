// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "camera.h"
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayCameraTokens,
    (horizontalAperture)
    (verticalAperture)
    (focalLength)
);

HdOSPRayCamera::HdOSPRayCamera(SdfPath const& id)
  : HdCamera(id)
{
}

HdOSPRayCamera::~HdOSPRayCamera() = default;

void HdOSPRayCamera::Sync(HdSceneDelegate *sceneDelegate,
                          HdRenderParam   *_renderParam,
                          HdDirtyBits     *dirtyBits)
{
  HdCamera::Sync(sceneDelegate, _renderParam, dirtyBits);
  auto renderParam = dynamic_cast<HdOSPRayRenderParam*>(_renderParam);

  auto horizontalAperture = sceneDelegate->GetCameraParamValue(GetId(), 
      HdOSPRayCameraTokens->horizontalAperture);
  auto verticalAperture = sceneDelegate->GetCameraParamValue(GetId(), 
      HdOSPRayCameraTokens->verticalAperture);
  auto focalLength = sceneDelegate->GetCameraParamValue(GetId(), 
      HdOSPRayCameraTokens->focalLength);

  if (horizontalAperture.IsHolding<float>()) {
      tmParams.active = vtActive.Get<float>();
  }
  if (vtExposure.IsHolding<float>()) {
      tmParams.exposure = vtExposure.Get<float>();
  }
  if (vtContrast.IsHolding<float>()) {
      tmParams.contrast = vtContrast.Get<float>();
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
