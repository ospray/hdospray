// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "instancer.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include "sampler.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/staticTokens.h>

#include <iostream>

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (instanceTransform)
    (rotate)
    (scale)
    (translate)
);
// clang-format on

#if HD_API_VERSION < 36
HdOSPRayInstancer::HdOSPRayInstancer(HdSceneDelegate* delegate,
                                     SdfPath const& id, SdfPath const& parentId)
    : HdInstancer(delegate, id, parentId)
#else
HdOSPRayInstancer::HdOSPRayInstancer(HdSceneDelegate* delegate,
                                     SdfPath const& id)
    : HdInstancer(delegate, id)
#endif
{
}

HdOSPRayInstancer::~HdOSPRayInstancer()
{
    TF_FOR_ALL (it, _primvarMap) {
        delete it->second;
    }
    _primvarMap.clear();
}

#if HD_API_VERSION > 35
void
HdOSPRayInstancer::Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                        HdDirtyBits* dirtyBits)
{
    _UpdateInstancer(delegate, dirtyBits);

    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, GetId())) {
        _SyncPrimvars(delegate, *dirtyBits);
    }
}
#endif

#if HD_API_VERSION < 36
void
HdOSPRayInstancer::_SyncPrimvars()
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdChangeTracker& changeTracker
           = GetDelegate()->GetRenderIndex().GetChangeTracker();
    SdfPath const& id = GetId();

    int dirtyBits = changeTracker.GetInstancerDirtyBits(id);
    if (HdChangeTracker::IsAnyPrimvarDirty(dirtyBits, id)) {
        std::lock_guard<std::mutex> lock(_instanceLock);

        TfTokenVector primvarNames;
        HdPrimvarDescriptorVector primvars
               = GetDelegate()->GetPrimvarDescriptors(id,
                                                      HdInterpolationInstance);

        for (HdPrimvarDescriptor const& pv : primvars) {
            if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name)) {
                VtValue value = GetDelegate()->Get(id, pv.name);
                if (!value.IsEmpty()) {
                    _primvarMap[pv.name] = std::make_unique<HdVtBufferSource>(
                           new HdVtBufferSource(pv.name, value));
                }
            }
        }

        changeTracker.MarkInstancerClean(id);
    }
}
#else
void
HdOSPRayInstancer::_SyncPrimvars(HdSceneDelegate* delegate,
                                 HdDirtyBits dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    std::lock_guard<std::mutex> lock(_instanceLock);
    SdfPath const& id = GetId();
    HdPrimvarDescriptorVector primvars
           = delegate->GetPrimvarDescriptors(id, HdInterpolationInstance);

    for (HdPrimvarDescriptor const& pv : primvars) {
        if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name)) {
            VtValue value = delegate->Get(id, pv.name);
            if (!value.IsEmpty()) {
                if (_primvarMap.count(pv.name) > 0) {
                    delete _primvarMap[pv.name];
                }
                _primvarMap[pv.name] = new HdVtBufferSource(pv.name, value);
            }
        }
    }
}
#endif

VtMatrix4dArray
HdOSPRayInstancer::ComputeInstanceTransforms(SdfPath const& prototypeId)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    GfMatrix4d instancerTransform
           = GetDelegate()->GetInstancerTransform(GetId());
    VtIntArray instanceIndices
           = GetDelegate()->GetInstanceIndices(GetId(), prototypeId);

    VtMatrix4dArray transforms(instanceIndices.size());
    for (size_t i = 0; i < instanceIndices.size(); ++i) {
        transforms[i] = instancerTransform;
    }

    // translate
    if (_primvarMap.count(_tokens->translate) > 0) {
        HdOSPRayBufferSampler sampler(*_primvarMap[_tokens->translate]);
        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            GfVec3f translate;
            if (sampler.Sample(instanceIndices[i], &translate)) {
                GfMatrix4d translateMat(1);
                translateMat.SetTranslate(GfVec3d(translate));
                transforms[i] = translateMat * transforms[i];
            }
        }
    }

    // rotate by quaternion
    if (_primvarMap.count(_tokens->rotate) > 0) {
        HdOSPRayBufferSampler sampler(*_primvarMap[_tokens->rotate]);
        for (size_t i = 0; i < instanceIndices.size(); ++i) {
#if HD_API_VERSION > 35
            GfQuath quath;
            if (sampler.Sample(instanceIndices[i], &quath)) {
                GfMatrix4d rotateMat(1);
                GfQuatd quat(quath.GetReal(), quath.GetImaginary());
                rotateMat.SetRotate(quat);
                transforms[i] = rotateMat * transforms[i];
            }
#else
            GfVec4f quat;
            if (sampler.Sample(instanceIndices[i], &quat)) {
                GfMatrix4d rotateMat(1);
                rotateMat.SetRotate(GfRotation(GfQuaternion(
                       quat[0], GfVec3d(quat[1], quat[2], quat[3]))));
                transforms[i] = rotateMat * transforms[i];
            }
#endif
        }
    }

    // scale
    if (_primvarMap.count(_tokens->scale) > 0) {
        HdOSPRayBufferSampler sampler(*_primvarMap[_tokens->scale]);
        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            GfVec3f scale;
            if (sampler.Sample(instanceIndices[i], &scale)) {
                GfMatrix4d scaleMat(1);
                scaleMat.SetScale(GfVec3d(scale));
                transforms[i] = scaleMat * transforms[i];
            }
        }
    }

    // transform
    if (_primvarMap.count(_tokens->instanceTransform) > 0) {
        HdOSPRayBufferSampler sampler(*_primvarMap[_tokens->instanceTransform]);
        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            GfMatrix4d instanceTransform;
            if (sampler.Sample(instanceIndices[i], &instanceTransform)) {
                transforms[i] = instanceTransform * transforms[i];
            }
        }
    }

    if (GetParentId().IsEmpty()) {
        return transforms;
    }

    HdInstancer* parentInstancer
           = GetDelegate()->GetRenderIndex().GetInstancer(GetParentId());
    if (!TF_VERIFY(parentInstancer)) {
        return transforms;
    }

    // compute nested transforms of the form parent * local
    VtMatrix4dArray parentTransforms
           = static_cast<HdOSPRayInstancer*>(parentInstancer)
                    ->ComputeInstanceTransforms(GetId());

    VtMatrix4dArray final(parentTransforms.size() * transforms.size());
    for (size_t i = 0; i < parentTransforms.size(); ++i) {
        for (size_t j = 0; j < transforms.size(); ++j) {
            final[i * transforms.size() + j]
                   = transforms[j] * parentTransforms[i];
        }
    }
    return final;
}