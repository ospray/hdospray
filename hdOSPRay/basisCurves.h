// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/basisCurves.h>
#include <pxr/imaging/hd/enums.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <mutex>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayRenderParam;

/// \class HdOSPRayBasisCurves
class HdOSPRayBasisCurves : public HdBasisCurves {
public:
    HF_MALLOC_TAG_NEW("new HdOSPRayBasisCurves");

    HdOSPRayBasisCurves(SdfPath const& id,
                        SdfPath const& instancerId = SdfPath());
    virtual ~HdOSPRayBasisCurves() = default;

    virtual void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                      HdDirtyBits* dirtyBits,
                      TfToken const& reprToken) override;

    virtual HdDirtyBits GetInitialDirtyBitsMask() const override;

    virtual void Finalize(HdRenderParam* renderParam) override
    {
    }

    void AddOSPInstances(std::vector<opp::Instance>& instanceList) const;

protected:
    virtual void _InitRepr(TfToken const& reprToken,
                           HdDirtyBits* dirtyBits) override;

    virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    void _UpdatePrimvarSources(HdSceneDelegate* sceneDelegate,
                               HdDirtyBits dirtyBits);

    void _UpdateOSPRayRepr(HdSceneDelegate* sceneDelegate,
                           TfToken const& reprToken,
                           HdDirtyBits* dirtyBitsState,
                           HdOSPRayRenderParam* renderParam);

private:
    opp::Geometry _ospCurves;
    std::vector<opp::GeometricModel> _geometricModels;
    std::vector<opp::Instance> _ospInstances;

    std::vector<rkcommon::math::vec4f> _position_radii;
    HdBasisCurvesTopology _topology;
    VtIntArray _indices;
    VtFloatArray _widths;
    VtVec3fArray _points;
    VtVec3fArray _normals;
    GfMatrix4f _xfm{1};
    VtVec2fArray _texcoords;
    VtVec4fArray _colors;
    GfVec4f _singleColor { .5f, .5f, .5f, 1.f };
    bool _populated { false };
};
