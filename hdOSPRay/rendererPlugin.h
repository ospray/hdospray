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

#if PXR_MINOR_VERSION >= 23
    virtual bool IsSupported(bool gpuEnabled) const override;
#else
    virtual bool IsSupported() const override;
#endif

private:
    HdOSPRayRendererPlugin(const HdOSPRayRendererPlugin&) = delete;
    HdOSPRayRendererPlugin& operator=(const HdOSPRayRendererPlugin&) = delete;
};
