// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "renderParam.h"
#include <memory>
#include <pxr/imaging/hd/camera.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdOSPRayCamera final : public HdCamera {
public:
    HdOSPRayCamera(SdfPath const& id);
    ~HdOSPRayCamera();

    void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override final;
};

PXR_NAMESPACE_CLOSE_SCOPE
