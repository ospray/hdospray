// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_USING_DIRECTIVE

///
/// \class HdOSPRayRendererPlugin
class HdOSPRayRendererPlugin final : public HdRendererPlugin {
public:
    HdOSPRayRendererPlugin() = default;
    virtual ~HdOSPRayRendererPlugin() = default;

    virtual HdRenderDelegate* CreateRenderDelegate() override;

    virtual void
    DeleteRenderDelegate(HdRenderDelegate* renderDelegate) override;

/// Checks to see if the HdOSPRay plugin is supported on the running system
///
#if PXR_VERSION < 2302
    bool IsSupported() const override;
#else
    bool IsSupported(bool gpuEnabled = true) const override;
#endif

private:
    HdOSPRayRendererPlugin(const HdOSPRayRendererPlugin&) = delete;
    HdOSPRayRendererPlugin& operator=(const HdOSPRayRendererPlugin&) = delete;
};