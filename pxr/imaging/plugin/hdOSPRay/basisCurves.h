//
// Copyright 2021 Intel
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef HDOSPRAY_BASISCURVES_H
#define HDOSPRAY_BASISCURVES_H

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

    /// Release any resources this class is holding onto: in this case,
    /// destroy the geometry object in the ospray scene graph.
    ///   \param renderParam An HdOSPRayRenderParam object containing top-level
    ///                      ospray state.
    virtual void Finalize(HdRenderParam* renderParam) override
    {
    }

    void AddOSPInstances(std::vector<opp::Instance>& instanceList) const;

protected:
    virtual void _InitRepr(TfToken const& reprToken,
                           HdDirtyBits* dirtyBits) override;

    virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    // Populate _primvarSourceMap (our local cache of primvar data) based on
    // scene data. Primvars will be turned into samplers in _PopulateRtMesh,
    // through the help of the _CreatePrimvarSampler() method.
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
    GfMatrix4f _xfm;
    VtVec2fArray _texcoords;
    VtVec4fArray _colors;
    GfVec4f _singleColor { .5f, .5f, .5f, 1.f };
    bool _populated { false };
};

#endif