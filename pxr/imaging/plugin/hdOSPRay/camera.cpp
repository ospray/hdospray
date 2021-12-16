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

  if (vtActive.IsHolding<bool>()
   || vtExposure.IsHolding<float>()
   || vtContrast.IsHolding<float>()
   || vtShoulder.IsHolding<float>()
   || vtMidIn.IsHolding<float>()
   || vtMidOut.IsHolding<float>()
   || vtHdrMax.IsHolding<float>())
  {
    ToneMappingParams tmParams = renderParam->GetToneMappingParams();

    if (vtActive.IsHolding<bool>()) {
      tmParams.active = vtActive.Get<bool>();
    }
    if (vtExposure.IsHolding<float>()) {
      tmParams.exposure = vtExposure.Get<float>();
    }
    if (vtContrast.IsHolding<float>()) {
      tmParams.contrast = vtContrast.Get<float>();
    }
    if (vtShoulder.IsHolding<float>()) {
      tmParams.shoulder = vtShoulder.Get<float>();
    }
    if (vtMidIn.IsHolding<float>()) {
      tmParams.midIn = vtMidIn.Get<float>();
    }
    if (vtMidOut.IsHolding<float>()) {
      tmParams.midOut = vtMidOut.Get<float>();
    }
    if (vtHdrMax.IsHolding<float>()) {
      tmParams.hdrMax = vtHdrMax.Get<float>();
    }

    renderParam->SetToneMappingParams(tmParams);
  }

}

PXR_NAMESPACE_CLOSE_SCOPE
