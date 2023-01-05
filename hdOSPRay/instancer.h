// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/pxr.h>

#include <pxr/imaging/hd/instancer.h>
#include <pxr/imaging/hd/vtBufferSource.h>

#include <pxr/base/tf/hashmap.h>
#include <pxr/base/tf/token.h>

#include <mutex>

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayInstancer : public HdInstancer {
public:
#if HD_API_VERSION < 36
    HdOSPRayInstancer(HdSceneDelegate* delegate, SdfPath const& id,
                      SdfPath const& parentId);
#else
    HdOSPRayInstancer(HdSceneDelegate* delegate, SdfPath const& id);
#endif

    ~HdOSPRayInstancer();

    virtual void Finalize(HdRenderParam* renderParam) override
    {
    }

#if HD_API_VERSION > 35
    void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override;
#endif

    VtMatrix4dArray ComputeInstanceTransforms(SdfPath const& prototypeId);

private:
#if HD_API_VERSION < 36
    void _SyncPrimvars();
#else
    void _SyncPrimvars(HdSceneDelegate* delegate, HdDirtyBits dirtyBits);
#endif

    std::mutex _instanceLock;

    // map of primvar name to data buffer
    TfHashMap<TfToken, HdVtBufferSource*, TfToken::HashFunctor> _primvarMap;
};