// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/pxr.h>

#include "sampler.h"

#include <pxr/base/gf/matrix4f.h>

PXR_NAMESPACE_USING_DIRECTIVE

/// \class HdOSPRayPrototypeContext
///
/// A small bit of state attached to each bit of prototype geometry in OSPRay,
/// for the benefit of HdOSPRayRenderPass::_TraceRay.
///
struct HdOSPRayPrototypeContext {
    /// A pointer back to the owning HdOSPRay rprim.
    HdRprim* rprim;
};

///
/// \class HdOSPRayInstanceContext
///
/// A small bit of state attached to each bit of instanced geometry in OSPRay
///
struct HdOSPRayInstanceContext {
    /// The object-to-world transform, for transforming normals to worldspace.
    GfMatrix4f objectToWorldMatrix;
    /// The scene the prototype geometry lives in, for passing to
};