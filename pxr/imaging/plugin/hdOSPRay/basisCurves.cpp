
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
#include "pxr/imaging/hdOSPRay/basisCurves.h"
#include "pxr/imaging/hdOSPRay/config.h"
#include "pxr/imaging/hdOSPRay/context.h"
#include "pxr/imaging/hdOSPRay/instancer.h"
#include "pxr/imaging/hdOSPRay/material.h"
#include "pxr/imaging/hdOSPRay/renderParam.h"
#include "pxr/imaging/hdOSPRay/renderPass.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/smoothNormals.h"
#include "pxr/imaging/pxOsd/tokens.h"

#include "rkcommon/math/AffineSpace.h"

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

HdOSPRayBasisCurves::HdOSPRayBasisCurves(SdfPath const& id,
                                         SdfPath const& instancerId)
    : HdBasisCurves(id, instancerId) {
    std::cout << "hdospBC::()\n";
                                         }

HdOSPRayBasisCurves::~HdOSPRayBasisCurves() {}

HdDirtyBits
HdOSPRayBasisCurves::GetInitialDirtyBitsMask() const
{
    std::cout << "hdospBC::GetInitialDirtyBitsMask()\n";
    HdDirtyBits mask = HdChangeTracker::Clean
        | HdChangeTracker::InitRepr
        | HdChangeTracker::DirtyExtent
        | HdChangeTracker::DirtyNormals
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyPrimID
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyDisplayStyle
        | HdChangeTracker::DirtyRepr
        | HdChangeTracker::DirtyMaterialId
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform 
        | HdChangeTracker::DirtyVisibility 
        | HdChangeTracker::DirtyWidths
        | HdChangeTracker::DirtyComputationPrimvarDesc
        ;

    if (!GetInstancerId().IsEmpty()) {
        mask |= HdChangeTracker::DirtyInstancer;
    }

    return (HdDirtyBits)mask;
}

void
HdOSPRayBasisCurves::_InitRepr(TfToken const& reprToken,
                               HdDirtyBits* dirtyBits)
{
    std::cout << "hdospBC::Init()\n";
    TF_UNUSED(reprToken);
    TF_UNUSED(dirtyBits);
}

HdDirtyBits
HdOSPRayBasisCurves::_PropagateDirtyBits(HdDirtyBits bits) const
{
    std::cout << "hdospBC::propagateDirtyBits\n";
    return bits;
}

void
HdOSPRayBasisCurves::Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                          HdDirtyBits* dirtyBits,
                          TfToken const& reprToken)
{
    std::cout << "hdospBC::Sync" << std::endl;

    // Pull top-level OSPRay state out of the render param.
    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    opp::Renderer renderer = ospRenderParam->GetOSPRayRenderer();

    SdfPath const& id = GetId();
    bool updateGeometry = false;
    bool isTransformDirty = false;
    if (*dirtyBits & HdChangeTracker::DirtyPoints) {
        _points = delegate->Get(id, HdTokens->points).Get<VtVec3fArray>();
        updateGeometry = true;
    }
    if (*dirtyBits & HdChangeTracker::DirtyTopology) {
        _topology = delegate->GetBasisCurvesTopology(id);
        if (_topology.HasIndices()) {
            std::cout << "hasIndices\n";
            _indices = _topology.GetCurveIndices();
        }
        updateGeometry = true;
    }
    if (*dirtyBits & HdChangeTracker::DirtyWidths) {
        _widths = delegate->Get(id, HdTokens->widths).Get<VtFloatArray>();
    }        
    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
    }
    if (*dirtyBits & HdChangeTracker::DirtyPrimvar) {
    }
    if (*dirtyBits & HdChangeTracker::DirtyTransform) {
        isTransformDirty = true;
    }
    if (*dirtyBits & HdChangeTracker::DirtyVisibility) {
    }

    if (updateGeometry) {
       _UpdateOSPRayRepr(delegate, reprToken, dirtyBits, ospRenderParam);
    }

    if ((HdChangeTracker::IsInstancerDirty(*dirtyBits, id) || isTransformDirty) && !_geometricModels.empty()) {
        _ospInstances.clear();
        if (!GetInstancerId().IsEmpty()) {
            std::cout << "hdosp::adding instancer\n";
            // Retrieve instance transforms from the instancer.
            HdRenderIndex& renderIndex = delegate->GetRenderIndex();
            HdInstancer* instancer = renderIndex.GetInstancer(GetInstancerId());
            VtMatrix4dArray transforms
                   = static_cast<HdOSPRayInstancer*>(instancer)
                            ->ComputeInstanceTransforms(GetId());

            size_t newSize = transforms.size();

            opp::Group group;
            group.setParam("geometry", opp::CopiedData(*_geometricModel));
            group.commit();

            // TODO: CARSON: reform instancer for ospray2
            _ospInstances.reserve(newSize);
            for (size_t i = 0; i < newSize; i++) {
                // Create the new instance.
                opp::Instance instance(group);

                // Combine the local transform and the instance transform.
                GfMatrix4f matf = _xfm * GfMatrix4f(transforms[i]);
                float* xfmf = matf.GetArray();
                affine3f xfm(vec3f(xfmf[0], xfmf[1], xfmf[2]),
                             vec3f(xfmf[4], xfmf[5], xfmf[6]),
                             vec3f(xfmf[8], xfmf[9], xfmf[10]),
                             vec3f(xfmf[12], xfmf[13], xfmf[14]));
                instance.setParam("xfm", xfm);
                instance.commit();
                _ospInstances.push_back(instance);
            }
        }
        // Otherwise, create our single instance (if necessary) and update
        // the transform (if necessary).
        else {
            std::cout << "hdosp::adding single instance\n";
            opp::Group group;
            group.setParam("geometry", opp::CopiedData(_geometricModels));
            group.commit();
            opp::Instance instance(group);
            // TODO: do we need to check for a local transform as well?
            GfMatrix4f matf = _xfm;
            float* xfmf = matf.GetArray();
            affine3f xfm(vec3f(xfmf[0], xfmf[1], xfmf[2]),
                         vec3f(xfmf[4], xfmf[5], xfmf[6]),
                         vec3f(xfmf[8], xfmf[9], xfmf[10]),
                         vec3f(xfmf[12], xfmf[13], xfmf[14]));
            // instance.setParam("xfm", xfm);
            instance.commit();
            _ospInstances.push_back(instance);
        }

        ospRenderParam->UpdateModelVersion();
    }

    // Clean all dirty bits.
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void
HdOSPRayBasisCurves::_UpdateOSPRayRepr(HdSceneDelegate* sceneDelegate,
                                 TfToken const& reprToken,
                                 HdDirtyBits* dirtyBitsState,
                               HdOSPRayRenderParam* renderParam)
{
    std::cout << "hdosprayBC::updateosprep" << std::endl;
    bool valid = false;
    if (_points.empty()) {
        std::cout << "points empty\n";
        TF_CODING_ERROR("_UpdateOSPRayRepr: points empty");
    } else if (_widths.empty()) {
        std::cout << "widths empty\n";
        TF_CODING_ERROR("_UpdateOSPRayRepr: widths empty");
    } //else if (_indices.empty()) TF_RUNTIME_ERROR("_UpdateOSPRayRepr: indices empty");
    else if (_points.size() != _widths.size()) {
        std::cout << "points != widths\n";
        TF_CODING_ERROR("_UpdateOSPRayRepr: points != widths");
    } else
        valid = true;

    if (!valid) {
      std::cout << "invalid\n";
      return;
    }

    

    std::cout << "curves points and witdths sizes: " << 
        _points.size() << " " << _widths.size() << std::endl;


    _ospCurves = opp::Geometry("curve");
    _position_radii.clear();

    for (size_t i = 0; i < _points.size(); i++) {
        _position_radii.emplace_back(vec4f({_points[i][0], _points[i][1], _points[i][2],
            _widths[i]}));
    }

    opp::SharedData vertices = opp::SharedData(_position_radii.data(), OSP_VEC4F,
        _position_radii.size());
    vertices.commit();
    _ospCurves.setParam("vertex.position_radius", vertices);

    auto type = _topology.GetCurveType();
    auto basis = _topology.GetCurveBasis();
    if (_indices.empty()) {
        _indices.reserve(_topology.GetNumCurves());
        auto vertexCounts = _topology.GetCurveVertexCounts();
        size_t index = 0;
        for(auto vci : vertexCounts) {
          std::vector<unsigned int> indices;
          std::cout << "vertexCount: " << vci << std::endl;
          for (size_t i = 0; i < vci - 3; i++) {
              indices.push_back(index++);
          }
          index += 3;
          auto geometry = opp::Geometry("curve");
          geometry.setParam("vertex.position_radius", vertices);
          geometry.setParam("index", opp::CopiedData(indices));
          geometry.setParam("type", OSP_FLAT);
          if (basis == HdTokens->bSpline)
              geometry.setParam("basis", OSP_BSPLINE);
          else if (basis == HdTokens->catmullRom)
              geometry.setParam("basis", OSP_CATMULL_ROM);
          else
              TF_RUNTIME_ERROR("hdospBS::sync: unsupported curve basis");
          geometry.commit();
          auto ospMaterial
                 = HdOSPRayMaterial::CreateDefaultMaterial(_singleColor);

          // Create new OSP Mesh
          auto gm = opp::GeometricModel(geometry);

          gm.setParam("material", ospMaterial);
          gm.commit();
          _geometricModels.push_back(gm);
        }
        // for(size_t i = 0 ; i < vertexCounts[0]-3; i++) {
        //     _indices.push_back(i);
        // }
        // for(size_t i = 0; i < _topology.GetNumCurves(); i++) {
        //     _indices.push_back(i);
        //     // _indices.push_back(i+1);
        //     // _indices.push_back(i+2);
        //     // _indices.push_back(i+3);
        // }
    }
    // opp::SharedData indices = opp::SharedData(
    //     _indices.data(), OSP_UINT, _indices.size());
    // indices.commit();
    // std::cout << "curves topology type: " << _topology.GetCurveType().GetString() << std::endl;
    // std::cout << "curves topology basis: " << _topology.GetCurveBasis().GetString() << std::endl;
    // std::cout << "curves wrap: " << _topology.GetCurveWrap().GetString() << std::endl;
    // _ospCurves.setParam("index", indices);
    // _ospCurves.setParam("type", OSP_FLAT);
    // if (basis == HdTokens->bSpline)
    //     _ospCurves.setParam("basis", OSP_BSPLINE);
    // else if (basis == HdTokens->catmullRom)
    //     _ospCurves.setParam("basis", OSP_CATMULL_ROM);
    // else
    //     TF_RUNTIME_ERROR("hdospBS::sync: unsupported curve basis");
    // _ospCurves.commit();

    // auto ospMaterial = HdOSPRayMaterial::CreateDefaultMaterial(_singleColor);

    // // Create new OSP Mesh
    // if (_geometricModel)
    //     delete _geometricModel;
    // _geometricModel = new opp::GeometricModel(_ospCurves);

    // _geometricModel->setParam("material", ospMaterial);
    // _geometricModel->commit();

    renderParam->UpdateModelVersion();

    if (!_populated) {
        renderParam->AddHdOSPRayBasisCurves(this);
        _populated = true;
    }
}

void
HdOSPRayBasisCurves::AddOSPInstances(std::vector<opp::Instance>& instanceList) const
{
    if (IsVisible()) {
    std::cout << "hdospbc::addospinstances: " << _ospInstances.size() << std::endl;
        instanceList.insert(instanceList.end(), _ospInstances.begin(),
                            _ospInstances.end());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE