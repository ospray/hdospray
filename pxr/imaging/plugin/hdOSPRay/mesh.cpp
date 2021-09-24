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
#include "mesh.h"
#include "config.h"
#include "context.h"
#include "instancer.h"
#include "material.h"
#include "renderParam.h"
#include "renderPass.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <rkcommon/math/AffineSpace.h>

using namespace rkcommon::math;

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayTokens,
    (st)
);
// clang-format on

HdOSPRayMesh::HdOSPRayMesh(SdfPath const& id, SdfPath const& instancerId)
    : HdMesh(id, instancerId)
    , _ospMesh(nullptr)
    , _geometricModel(nullptr)
    , _adjacencyValid(false)
    , _normalsValid(false)
    , _refined(false)
    , _cullStyle(HdCullStyleDontCare)
{
}

HdOSPRayMesh::~HdOSPRayMesh()
{
    delete _geometricModel;
}

void
HdOSPRayMesh::Finalize(HdRenderParam* renderParam)
{
}

HdDirtyBits
HdOSPRayMesh::GetInitialDirtyBitsMask() const
{
    // The initial dirty bits control what data is available on the first
    // run through _PopulateMesh(), so it should list every data item
    // that _PopulateMesh requests.
    int mask = HdChangeTracker::Clean | HdChangeTracker::InitRepr
           | HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyTopology
           | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility
           | HdChangeTracker::DirtyCullStyle | HdChangeTracker::DirtyDoubleSided
           | HdChangeTracker::DirtyDisplayStyle
           | HdChangeTracker::DirtySubdivTags | HdChangeTracker::DirtyPrimvar
           | HdChangeTracker::DirtyNormals | HdChangeTracker::DirtyInstancer
           | HdChangeTracker::DirtyPrimID | HdChangeTracker::DirtyRepr
           | HdChangeTracker::DirtyMaterialId;

    return (HdDirtyBits)mask;
}

HdDirtyBits
HdOSPRayMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void
HdOSPRayMesh::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
    TF_UNUSED(dirtyBits);

    // Create an empty repr.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    if (it == _reprs.end()) {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

static bool
_IsEnabledForceQuadrangulate()
{
    static bool enabled = HdOSPRayConfig::GetInstance().forceQuadrangulate == 1;
    return enabled;
}

bool
HdOSPRayMesh::_UseQuadIndices(const HdRenderIndex& renderIndex,
                              HdMeshTopology const& topology) const
{
    // We should never quadrangulate for subdivision schemes
    // which refine to triangles (like Loop)
    if (topology.GetScheme() == PxOsdOpenSubdivTokens->loop)
        return false;

    // According to HdSt _ospMesh.cpp, always use quads on surfaces with ptex
    const HdOSPRayMaterial* material = static_cast<const HdOSPRayMaterial*>(
           renderIndex.GetSprim(HdPrimTypeTokens->material, GetMaterialId()));
    if (material && material->HasPtex())
        return true;

    // Fallback to the environment variable, which allows forcing of
    // quadrangulation for debugging/testing.
    return _IsEnabledForceQuadrangulate();
}

void
HdOSPRayMesh::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
                   HdDirtyBits* dirtyBits, TfToken const& reprToken)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX: Meshes can have multiple reprs; this is done, for example, when
    // the drawstyle specifies different rasterizing modes between front faces
    // and back faces. With raytracing, this concept makes less sense, but
    // combining semantics of two HdMeshReprDesc is tricky in the general case.
    // For now, HdOSPRayMesh only respects the first desc; this should be fixed.
    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const HdMeshReprDesc& desc = descs[0];

    // Pull top-level OSPRay state out of the render param.
    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    opp::Renderer renderer = ospRenderParam->GetOSPRayRenderer();

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(),
                       sceneDelegate->GetMaterialId(GetId()));
    }

    // Create ospray geometry objects.
    _PopulateOSPMesh(sceneDelegate, renderer, dirtyBits, desc, ospRenderParam);

    if (*dirtyBits & HdChangeTracker::DirtyTopology) {
        // TODO: update material here?
    }
}

void
HdOSPRayMesh::_UpdatePrimvarSources(HdSceneDelegate* sceneDelegate,
                                    HdDirtyBits dirtyBits)
{
    HD_TRACE_FUNCTION();
    SdfPath const& id = GetId();

    // Update _primvarSourceMap, our local cache of raw primvar data.
    // This function pulls data from the scene delegate, but defers processing.
    //
    // While iterating primvars, we skip "points" (vertex positions) because
    // the points primvar is processed by _PopulateMesh. We only call
    // GetPrimvar on primvars that have been marked dirty.
    //
    // Currently, hydra doesn't have a good way of communicating changes in
    // the set of primvars, so we only ever add and update to the primvar set.

    HdPrimvarDescriptorVector primvars;
    for (size_t i = 0; i < HdInterpolationCount; ++i) {
        HdInterpolation interp = static_cast<HdInterpolation>(i);
        primvars = GetPrimvarDescriptors(sceneDelegate, interp);
        for (HdPrimvarDescriptor const& pv : primvars) {

            // Points are handled outside _UpdatePrimvarSources
            if (pv.name == HdTokens->points)
                continue;

            auto value = sceneDelegate->Get(id, pv.name);

            // TODO: need to find a better way to identify the primvar for
            // the texture coordinates
            // texcoords
            if ((pv.name == "Texture_uv" || pv.name == "st")
                && HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                   HdOSPRayTokens->st)) {
                if (value.IsHolding<VtVec2fArray>()) {
                    _texcoords = value.Get<VtVec2fArray>();
                }
            }

            if (pv.name == "displayColor"
                && HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                   HdTokens->displayColor)) {
                // Create 4 component displayColor/opacity array for OSPRay
                // XXX OSPRay currently expects 4 component color array.
                // XXX Extend OSPRay to support separate RGB/Opacity arrays
                if (value.IsHolding<VtVec3fArray>()) {
                    const VtVec3fArray& colors = value.Get<VtVec3fArray>();
                    _colors.resize(colors.size());
                    for (size_t i = 0; i < colors.size(); i++) {
                        _colors[i] = { colors[i].data()[0], colors[i].data()[1],
                                       colors[i].data()[2], 1.f };
                    }
                    if (!_colors.empty()) {
                        // for vertex coloring, make diffuse color white
                        if (_colors.size() > 1) {
                            _singleColor = { 1.f, 1.f, 1.f, 1.f };
                        } else {
                            _singleColor = { _colors[0][0], _colors[0][1],
                                             _colors[0][2], 1.f };
                        }
                    }
                }
            }

            if (pv.name == "displayOpacity"
                && HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                   HdTokens->displayOpacity)) {
                // XXX assuming displayOpacity can't exist without
                // displayColor and/or have a different size
                if (value.IsHolding<VtFloatArray>()) {
                    const VtFloatArray& opacities = value.Get<VtFloatArray>();
                    if (_colors.size() < opacities.size())
                        _colors.resize(opacities.size());
                    for (size_t i = 0; i < opacities.size(); i++)
                        _colors[i].data()[3] = opacities[i];
                    if (!_colors.empty())
                        _singleColor[3] = _colors[0][3];
                }
            }
        }
    }
}

void
HdOSPRayMesh::_PopulateOSPMesh(HdSceneDelegate* sceneDelegate,
                               opp::Renderer renderer, HdDirtyBits* dirtyBits,
                               HdMeshReprDesc const& desc,
                               HdOSPRayRenderParam* renderParam)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const& id = GetId();
    bool isTransformDirty = false;
    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        VtValue value = sceneDelegate->Get(id, HdTokens->points);
        _points = value.Get<VtVec3fArray>();
        if (_points.size() > 0) {
            _normalsValid = false;
        }
    }

    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);
        _topology = HdMeshTopology(_topology, displayStyle.refineLevel);
    }

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        _transform = GfMatrix4f(sceneDelegate->GetTransform(id));
        isTransformDirty = true;
    }

    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        _UpdateVisibility(sceneDelegate, dirtyBits);
        renderParam->UpdateModelVersion();
    }

    if (HdChangeTracker::IsCullStyleDirty(*dirtyBits, id)) {
        _cullStyle = GetCullStyle(sceneDelegate);
    }
    if (HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
        _doubleSided = IsDoubleSided(sceneDelegate);
    }
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->widths)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->primvar)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id,
                                           HdTokens->displayColor)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id,
                                           HdTokens->displayOpacity)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id,
                                           HdOSPRayTokens->st)) {
        _UpdatePrimvarSources(sceneDelegate, *dirtyBits);
    }

    ////////////////////////////////////////////////////////////////////////
    // 2. Resolve drawstyles

    // The repr defines a set of geometry styles for drawing the mesh
    // (see hd/enums.h). We're ignoring points and wireframe for now, so
    // HdMeshGeomStyleSurf maps to subdivs and everything else maps to
    // HdMeshGeomStyleHull (coarse triangulated mesh).
    bool doRefine = (desc.geomStyle == HdMeshGeomStyleSurf);

    // If the subdivision scheme is "none", force us to not refine.
    doRefine
           = doRefine && (_topology.GetScheme() != PxOsdOpenSubdivTokens->none);

    // If the refine level is 0, triangulate instead of subdividing.
    doRefine = doRefine && (_topology.GetRefineLevel() > 0);

    // The repr defines whether we should compute smooth normals for this mesh:
    // per-vertex normals taken as an average of adjacent faces, and
    // interpolated smoothly across faces.
    _smoothNormals = !desc.flatShadingEnabled;

    // If the subdivision scheme is "none" or "bilinear", force us not to use
    // smooth normals.
    _smoothNormals = _smoothNormals
           && ((_topology.GetScheme() != PxOsdOpenSubdivTokens->none));

    if (HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id)
        && _topology.GetRefineLevel() > 0) {
        _topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }

    ////////////////////////////////////////////////////////////////////////
    // 3. Populate ospray prototype object.

    // If the refine level changed or the mesh was recreated, we need to pass
    // the refine level into the ospray subdiv object.
    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        if (doRefine) {
            // Pass the target number of uniform refinements to Embree.
            // Embree refinement is specified as the number of quads to generate
            // per edge, whereas hydra refinement is the number of recursive
            // splits, so we need to pass embree (2^refineLevel).

            _tessellationRate = (1 << _topology.GetRefineLevel());
            // XXX: As of Embree 2.9.0, rendering with tessellation level 1
            // (i.e. coarse mesh) results in weird normals, so force at least
            // one level of subdivision.
            if (_tessellationRate == 1) {
                _tessellationRate++;
            }
        }
    }

    // If the topology has changed, or the value of doRefine has changed, we
    // need to create or recreate the OSPRay mesh object.
    // _GetInitialDirtyBits() ensures that the topology is dirty the first time
    // this function is called, so that the OSPRay mesh is always created.
    bool newMesh = false;
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)
        || doRefine != _refined) {
        newMesh = true;

        // Force the smooth normals code to rebuild the "normals" primvar the
        // next time smooth normals is enabled.
        _normalsValid = false;

        // When pulling a new topology, we don't want to overwrite the
        // refine level or subdiv tags, which are provided separately by the
        // scene delegate, so we save and restore them.
        PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
        int refineLevel = _topology.GetRefineLevel();
        _topology = HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _topology.SetSubdivTags(subdivTags);
        _adjacencyValid = false;
        // _geomSubsets = _topology.GetGeomSubsets();


        if (doRefine) {
            _ospMesh = _CreateOSPRaySubdivMesh();
            _ospMesh.commit();

            HdMeshUtil meshUtil(&_topology, GetId());
            meshUtil.ComputeQuadIndices(&_quadIndices, &_quadPrimitiveParams);
            // Check if texcoords are provides as face varying.
            // XXX: (This code currently only cares about _texcoords, but
            // should be generalized to all primvars)
            bool faceVaryingTexcoord = false;
            HdPrimvarDescriptorVector faceVaryingPrimvars
                   = GetPrimvarDescriptors(sceneDelegate,
                                           HdInterpolationFaceVarying);
            for (HdPrimvarDescriptor const& pv : faceVaryingPrimvars) {
                if (pv.name == "Texture_uv" || pv.name == "st")
                    faceVaryingTexcoord = true;
            }

            if (faceVaryingTexcoord) {
                TfToken buffName = HdOSPRayTokens->st;
                VtValue buffValue = VtValue(_texcoords);
                HdVtBufferSource buffer(buffName, buffValue);
                VtValue quadPrimvar;

                auto success = meshUtil.ComputeQuadrangulatedFaceVaryingPrimvar(
                       buffer.GetData(), buffer.GetNumElements(),
                       buffer.GetTupleType().type, &quadPrimvar);
                if (success && quadPrimvar.IsHolding<VtVec2fArray>()) {
                    _texcoords = quadPrimvar.Get<VtVec2fArray>();
                } else {
                    std::cout << "ERROR: could not quadrangulate face-varying "
                                 "data\n";
                }
            }
        }
        _refined = doRefine;
    }

    // Update the smooth normals in steps:
    // 1. If the topology is dirty, update the adjacency table, a processed
    //    form of the topology that helps calculate smooth normals quickly.
    // 2. If the points are dirty, update the smooth normal buffer itself.
    _normalsValid = false;
    if (_smoothNormals && !_adjacencyValid) {
        _adjacency.BuildAdjacencyTable(&_topology);
        _adjacencyValid = true;
        // If we rebuilt the adjacency table, force a rebuild of normals.
        _normalsValid = false;
    }
    if (_smoothNormals && !_normalsValid && !doRefine) {
        _computedNormals = Hd_SmoothNormals::ComputeSmoothNormals(
               &_adjacency, _points.size(), _points.cdata());
        _normalsValid = true;
    }

    if (newMesh
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id,
                                           HdOSPRayTokens->st)) {

        const HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
        // Check if texcoords are provides as face varying.
        // XXX: (This code currently only cares about _texcoords, but
        // should be generalized to all primvars)
        bool faceVaryingTexcoord = false;
        HdPrimvarDescriptorVector faceVaryingPrimvars = GetPrimvarDescriptors(
               sceneDelegate, HdInterpolationFaceVarying);
        for (HdPrimvarDescriptor const& pv : faceVaryingPrimvars) {
            if (pv.name == "Texture_uv" || pv.name == "st")
                faceVaryingTexcoord = true;
        }

        bool useQuads = _UseQuadIndices(renderIndex, _topology);

        if (!_refined) {
            if (useQuads) {
                HdMeshUtil meshUtil(&_topology, GetId());
                meshUtil.ComputeQuadIndices(&_quadIndices, &_quadPrimitiveParams);

                if (faceVaryingTexcoord) {
                    TfToken buffName = HdOSPRayTokens->st;
                    VtValue buffValue = VtValue(_texcoords);
                    HdVtBufferSource buffer(buffName, buffValue);
                    VtValue quadPrimvar;

                    auto success
                           = meshUtil.ComputeQuadrangulatedFaceVaryingPrimvar(
                                  buffer.GetData(), buffer.GetNumElements(),
                                  buffer.GetTupleType().type, &quadPrimvar);
                    if (success && quadPrimvar.IsHolding<VtVec2fArray>()) {
                        _texcoords = quadPrimvar.Get<VtVec2fArray>();
                    } else {
                        std::cout
                               << "ERROR: could not quadrangulate face-varying "
                                  "data\n";
                    }

                    // usd stores texcoords in face indexed -> each quad has 4
                    // unique texcoords.
                    // let's try converting it to match our vertex indices
                    VtVec2fArray texcoords2;
                    texcoords2.resize(_points.size());
                    for (size_t q = 0; q < _quadIndices.size(); q++) {
                        for (int i = 0; i < 4; i++) {
                            // value at quadindex[q][i] maps to q*4+i texcoord;
                            const size_t tc1index = q * 4 + i;
                            const size_t tc2index = _quadIndices[q][i];
                            texcoords2[tc2index] = _texcoords[tc1index];
                        }
                    }
                    _texcoords = texcoords2;
                }
            } else {
                HdMeshUtil meshUtil(&_topology, GetId());
                meshUtil.ComputeTriangleIndices(&_triangulatedIndices,
                                                &_trianglePrimitiveParams);

                if (faceVaryingTexcoord) {
                    TfToken buffName = HdOSPRayTokens->st;
                    VtValue buffValue = VtValue(_texcoords);
                    HdVtBufferSource buffer(buffName, buffValue);
                    VtValue triangulatedPrimvar;

                    auto success
                           = meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
                                  buffer.GetData(), buffer.GetNumElements(),
                                  buffer.GetTupleType().type,
                                  &triangulatedPrimvar);
                    if (success
                        && triangulatedPrimvar.IsHolding<VtVec2fArray>()) {
                        _texcoords = triangulatedPrimvar.Get<VtVec2fArray>();
                    } else {
                        std::cout
                               << "ERROR: could not triangulate face-varying "
                                  "data\n";
                    }

                    // usd stores texcoords in face indexed -> each triangle has
                    // 3 unique texcoords. let's try converting it to match our
                    // vertex indices
                    VtVec2fArray texcoords2;
                    texcoords2.resize(_points.size());
                    for (size_t t = 0; t < _triangulatedIndices.size(); t++) {
                        for (int i = 0; i < 3; i++) {
                            // value at triangleIndex[t][i] maps to t*3+i
                            // texcoord;
                            const size_t tc1index = t * 3 + i;
                            const size_t tc2index = _triangulatedIndices[t][i];
                            texcoords2[tc2index] = _texcoords[tc1index];
                        }
                    }
                    _texcoords = texcoords2;
                }
            }
        }

        _geomSubsets = _topology.GetGeomSubsets();
        const HdOSPRayMaterial* subsetMaterial = nullptr;

        // static int count = 0;
        for(auto subset : _geomSubsets) {
            // if (count++ > 5)
            //     continue;
            if (!TF_VERIFY(subset.type == HdGeomSubset::TypeFaceSet))
                continue;
            VtVec3iArray triangulatedIndices;
            VtIntArray trianglePrimitiveParams;
            VtVec4iArray quadIndices;
            VtVec2iArray quadPrimitiveParams;
            // if (useQuads) {
            //     std::cout << "quad subset\n";
            //     HdMeshUtil meshUtil(&_topology, subset.id);
            //     meshUtil.ComputeQuadIndices(&quadIndices,
            //                                     &quadPrimitiveParams);
            //     if (quadIndices.empty())
            //         continue;
            // } else {
            //     std::cout << "triangle subset\n";
            //     HdMeshUtil meshUtil(&_topology, subset.id);
            //     meshUtil.ComputeTriangleIndices(&triangulatedIndices,
            //                                     &trianglePrimitiveParams);
            //     if (triangulatedIndices.empty())
            //         continue;
            // }
            //geomsubsets use face indices instead of vertex indices.
            //create new list of indices from these for ospray
            // std::vector<unsigned int> vertexIndices;
            // int vertexStride = (useQuads) ? 4 : 3;
            // for (auto i : subset.indices) {
            //     // unsigned int offset = i * vertexStride;
            //     for (int j = 0; j < vertexStride; j++) {
            //         if (useQuads) {
            //             auto index = _quadIndices[i][j];
            //             vertexIndices.push_back(index);
            //         } else {
            //             auto index = _triangulatedIndices[i][j];
            //             if (index < _points.size())
            //                 vertexIndices.push_back(index);
            //         }
            //     }
            // }

            // auto ospMesh = _CreateOSPRayMesh(vertexIndices, quadPrimitiveParams,
            // vertexIndices, trianglePrimitiveParams, faceVaryingTexcoord,
            // _texcoords, _points, _computedNormals, _colors, _refined, useQuads);

            // opp::Geometry ospMesh = opp::Geometry("mesh");
            // {
            //     VtVec3fArray& points = _points;
            //     VtVec3fArray& normals = _normals;
            //     VtVec4fArray& colors = _colors;
            //     VtVec2fArray& texcoords = _texcoords;
            //     if (!_refined) {
            //         if (useQuads) {
            //             opp::SharedData indices = opp::SharedData(
            //                    vertexIndices.data(), OSP_VEC4UI,
            //                    vertexIndices.size()/4);

            //             indices.commit();
            //             ospMesh.setParam("index", indices);
            //             std::cout
            //                    << "mesh indices of size: " << vertexIndices.size()
            //                    << std::endl;

            //         } else { // triangles
            //             opp::SharedData indices = opp::SharedData(
            //                    vertexIndices.data(), OSP_VEC3UI,
            //                    vertexIndices.size()/3);

            //             indices.commit();
            //             ospMesh.setParam("index", indices);
            //             std::cout << "mesh indices of size: "
            //                       << vertexIndices.size() << std::endl;
            //         }
            //     }

            //     opp::SharedData verticesData = opp::SharedData(
            //            points.cdata(), OSP_VEC3F, points.size());
            //     verticesData.commit();
            //     ospMesh.setParam("vertex.position", verticesData);

            //     if (normals.size()) {
            //         opp::SharedData normalsData = opp::SharedData(
            //                normals.cdata(), OSP_VEC3F, normals.size());
            //         normalsData.commit();
            //         ospMesh.setParam("vertex.normal", normalsData);
            //     }

            //     if (colors.size() > 1) {
            //         opp::SharedData colorsData = opp::SharedData(
            //                colors.cdata(), OSP_VEC4F, colors.size());
            //         colorsData.commit();
            //         ospMesh.setParam("vetex.color", colorsData);
            //     }

            //     if (texcoords.size() > 1) {
            //         opp::SharedData texcoordsData = opp::SharedData(
            //                texcoords.cdata(), OSP_VEC2F, texcoords.size());
            //         texcoordsData.commit();
            //         ospMesh.setParam("vertex.texcoord", texcoordsData);
            //     }
            // }
            // ospMesh.commit();

            const HdOSPRayMaterial* material
                = static_cast<const HdOSPRayMaterial*>(renderIndex.GetSprim(
                        HdPrimTypeTokens->material, subset.materialId));
            subsetMaterial = material;

            // opp::Material ospMaterial;

            // if (material && material->GetOSPRayMaterial()) {
            //     ospMaterial = material->GetOSPRayMaterial();
            // } else {
            //     // Create new ospMaterial
            //     ospMaterial = HdOSPRayMaterial::CreateDefaultMaterial(_singleColor);
            // }

            // auto geometricModel = opp::GeometricModel(ospMesh);

            // geometricModel.setParam("material", ospMaterial);
            // geometricModel.commit();
            // _geomSubsetModels.push_back(geometricModel);
            std::cout << "added subset geometric model\n";
        }

        if (_geomSubsetModels.empty()) {
            _ospMesh = _CreateOSPRayMesh(_quadIndices, _quadPrimitiveParams,
                _triangulatedIndices, _trianglePrimitiveParams, faceVaryingTexcoord,
                _texcoords, _points, _computedNormals, _colors, _refined, useQuads);
            _ospMesh.commit();

            const HdOSPRayMaterial* material = nullptr;
            if (subsetMaterial)
                material = subsetMaterial;
            else
                material = static_cast<const HdOSPRayMaterial*>(renderIndex.GetSprim(
                        HdPrimTypeTokens->material, GetMaterialId()));

            opp::Material ospMaterial;

            if (material && material->GetOSPRayMaterial()) {
                ospMaterial = material->GetOSPRayMaterial();
            } else {
                // Create new ospMaterial
                ospMaterial = HdOSPRayMaterial::CreateDefaultMaterial(_singleColor);
            }

            // Create new OSP Mesh
            if (_geometricModel)
                delete _geometricModel;
            _geometricModel = new opp::GeometricModel(_ospMesh);

            _geometricModel->setParam("material", ospMaterial);
            _ospMesh.commit();
            _geometricModel->commit();
        }

        renderParam->UpdateModelVersion();
    }

    ////////////////////////////////////////////////////////////////////////
    // 4. Populate ospray instance objects.

    // If the _ospMesh is instanced, create one new instance per transform.
    // XXX: The current instancer invalidation tracking makes it hard for
    // HdOSPRay to tell whether transforms will be dirty, so this code
    // pulls them every frame.

    if (HdChangeTracker::IsInstancerDirty(*dirtyBits, id) || isTransformDirty) {
        _ospInstances.clear();
        if (!GetInstancerId().IsEmpty()) {
            std::cout << "using instancer\n";
            // Retrieve instance transforms from the instancer.
            HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
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
                GfMatrix4f matf = _transform * GfMatrix4f(transforms[i]);
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
            std::cout << "single instance\n";
            opp::Group group;
            opp::Instance instance(group);
            // TODO: do we need to check for a local transform as well?
            GfMatrix4f matf = _transform;
            float* xfmf = matf.GetArray();
            affine3f xfm(vec3f(xfmf[0], xfmf[1], xfmf[2]),
                         vec3f(xfmf[4], xfmf[5], xfmf[6]),
                         vec3f(xfmf[8], xfmf[9], xfmf[10]),
                         vec3f(xfmf[12], xfmf[13], xfmf[14]));
            instance.setParam("xfm", xfm);
            instance.commit();
            if (_geomSubsetModels.size()) {
                std::cout << "pushing geomsubsets instance\n";
                group.setParam("geometry", opp::CopiedData(_geomSubsetModels));
            } else {
                // std::cout << "pushing geom instance\n";
                group.setParam("geometry", opp::CopiedData(*_geometricModel));
            }
            group.commit();
            _ospInstances.push_back(instance);
        }
        renderParam->UpdateModelVersion();
    }
    if (!_populated) {
        renderParam->AddHdOSPRayMesh(this);
        _populated = true;
    }

    // Clean all dirty bits.
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void
HdOSPRayMesh::AddOSPInstances(std::vector<opp::Instance>& instanceList) const
{
    if (IsVisible()) {
        instanceList.insert(instanceList.end(), _ospInstances.begin(),
                            _ospInstances.end());
    }
}

void
HdOSPRayMesh::_UpdateDrawItemGeometricShader(HdSceneDelegate* sceneDelegate,
                                             HdStDrawItem* drawItem,
                                             const HdMeshReprDesc& desc,
                                             size_t drawItemIdForDesc)
{
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();

    // resolve geom style, cull style
    HdCullStyle cullStyle = desc.cullStyle;

    // if the repr doesn't have an opinion about cullstyle, use the
    // prim's default (it could also be DontCare, then renderPass's
    // cullStyle is going to be used).
    //
    // i.e.
    //   Repr CullStyle > Rprim CullStyle > RenderPass CullStyle
    //
    if (cullStyle == HdCullStyleDontCare) {
        cullStyle = _cullStyle;
    }

    // The edge geomstyles below are rasterized as lines.
    // See HdSt_GeometricShader::BindResources()
    // The batches need to be validated and rebuilt if necessary.
    renderIndex.GetChangeTracker().MarkBatchesDirty();
}

opp::Geometry
HdOSPRayMesh::_CreateOSPRayMesh(VtVec4iArray& quadIndices, 
    VtVec2iArray& quadPrimitiveParams, VtVec3iArray& triangulatedIndices,
    VtIntArray trianglePrimitiveParams, bool faceVaryingTexcoord, VtVec2fArray& texcoords, 
    VtVec3fArray& points, VtVec3fArray& normals, VtVec4fArray& colors, bool refined,
    bool useQuads)
{
    opp::Geometry ospMesh = opp::Geometry("mesh");
    if (!_refined) {
        if (useQuads) {
            opp::SharedData indices = opp::SharedData(
                   quadIndices.cdata(), OSP_VEC4UI, quadIndices.size());

            indices.commit();
            ospMesh.setParam("index", indices);
            std::cout << "mesh indices of size: " << quadIndices.size() << std::endl;

        } else { // triangles
            opp::SharedData indices
                   = opp::SharedData(triangulatedIndices.cdata(), OSP_VEC3UI,
                                     triangulatedIndices.size());

            indices.commit();
            ospMesh.setParam("index", indices);
            std::cout << "mesh indices of size: " << triangulatedIndices.size() << std::endl;
        }
    }

    opp::SharedData verticesData
           = opp::SharedData(points.cdata(), OSP_VEC3F, points.size());
    verticesData.commit();
    ospMesh.setParam("vertex.position", verticesData);

    if (normals.size()) {
        opp::SharedData normalsData = opp::SharedData(
               normals.cdata(), OSP_VEC3F, normals.size());
        normalsData.commit();
        ospMesh.setParam("vertex.normal", normalsData);
    }

    if (colors.size() > 1) {
        opp::SharedData colorsData
               = opp::SharedData(colors.cdata(), OSP_VEC4F, colors.size());
        colorsData.commit();
        ospMesh.setParam("vetex.color", colorsData);
    }

    if (texcoords.size() > 1) {
        opp::SharedData texcoordsData = opp::SharedData(
               texcoords.cdata(), OSP_VEC2F, texcoords.size());
        texcoordsData.commit();
        ospMesh.setParam("vertex.texcoord", texcoordsData);
    }
    return ospMesh;
}

opp::Geometry
HdOSPRayMesh::_CreateOSPRaySubdivMesh()
{
    const PxOsdSubdivTags& subdivTags = _topology.GetSubdivTags();

    // This loop calculates the number of edge creases, in preparation for
    // unrolling the edge crease buffer below.
    VtIntArray const creaseLengths = subdivTags.GetCreaseLengths();
    int numEdgeCreases = 0;
    for (size_t i = 0; i < creaseLengths.size(); ++i) {
        numEdgeCreases += creaseLengths[i] - 1;
    }

    // For vertex creases, sanity check that the weights and indices
    // arrays are the same length.
    int numVertexCreases
           = static_cast<int>(subdivTags.GetCornerIndices().size());
    if (numVertexCreases
        != static_cast<int>(subdivTags.GetCornerWeights().size())) {
        TF_WARN("Mismatch between vertex crease indices and weights");
        numVertexCreases = 0;
    }

    auto mesh = opp::Geometry("subdivision");
    int numFaceVertices = _topology.GetFaceVertexCounts().size();
    int numIndices = _topology.GetFaceVertexIndices().size();
    int numVertices = _points.size();

    opp::SharedData vertices
           = opp::SharedData(_points.data(), OSP_VEC3F, numVertices);
    vertices.commit();
    mesh.setParam("vertex.position", vertices);
    opp::SharedData faces = opp::SharedData(
           _topology.GetFaceVertexCounts().data(), OSP_UINT, numFaceVertices);
    faces.commit();
    mesh.setParam("face", faces);
    opp::SharedData indices = opp::SharedData(
           _topology.GetFaceVertexIndices().data(), OSP_UINT, numIndices);
    indices.commit();
    // // TODO: need to handle subivion types correctly
    mesh.setParam("index", indices);

    // subdiv boundary mode
    TfToken const vertexRule
           = _topology.GetSubdivTags().GetVertexInterpolationRule();

    if (vertexRule == PxOsdOpenSubdivTokens->none) {
        mesh.setParam("mode", OSP_SUBDIVISION_NO_BOUNDARY);
    } else if (vertexRule == PxOsdOpenSubdivTokens->edgeOnly) {
        mesh.setParam("mode", OSP_SUBDIVISION_SMOOTH_BOUNDARY);
    } else if (vertexRule == PxOsdOpenSubdivTokens->edgeAndCorner) {
        mesh.setParam("mode", OSP_SUBDIVISION_PIN_CORNERS);
    } else if (vertexRule == PxOsdOpenSubdivTokens->all) {
        mesh.setParam("mode", OSP_SUBDIVISION_PIN_ALL);
    } else if (vertexRule == PxOsdOpenSubdivTokens->cornersOnly) {
        mesh.setParam("mode", OSP_SUBDIVISION_PIN_CORNERS);
    } else if (vertexRule == PxOsdOpenSubdivTokens->boundaries) {
        mesh.setParam("mode", OSP_SUBDIVISION_PIN_BOUNDARY);
    } else if (vertexRule == PxOsdOpenSubdivTokens->smooth) {
        mesh.setParam("mode", OSP_SUBDIVISION_SMOOTH_BOUNDARY);
    } else {
        if (!vertexRule.IsEmpty()) {
            TF_WARN("hdOSPRay: Unknown vertex interpolation rule: %s",
                    vertexRule.GetText());
        }
        mesh.setParam("mode", OSP_SUBDIVISION_PIN_CORNERS);
    }

    // TODO: set hole buffer

    // OSPData colorsData = nullptr;
    // there is a bug in ospray subd requiring a color array of size points.
    // if the color array is less than points size we create an array
    // of white colors as a workaround.
    if (_colors.size() < _points.size()) {
        vec4f white = { 1.f, 1.f, 1.f, 1.f };
        std::vector<vec4f> colorDummy(_points.size(), white);
        mesh.setParam("vertex.color", opp::CopiedData(colorDummy));
    } else {
        opp::CopiedData colorsData
               = opp::CopiedData(_colors.cdata(), OSP_VEC4F, _colors.size());
        colorsData.commit();
        mesh.setParam("vertex.color", colorsData);
    }
    // TODO: ospray subd appears to require color data... this will be fixed in
    // next release

    mesh.setParam("level", _tessellationRate);

    // If this topology has edge creases, unroll the edge crease buffer.
    if (numEdgeCreases > 0) {
        std::vector<unsigned int> ospCreaseIndices(numEdgeCreases * 2);
        std::vector<float> ospCreaseWeights(numEdgeCreases);
        int ospEdgeIndex = 0;

        VtIntArray const creaseIndices = subdivTags.GetCreaseIndices();
        VtFloatArray const creaseWeights = subdivTags.GetCreaseWeights();

        bool weightPerCrease = (creaseWeights.size() == creaseLengths.size());

        // Loop through the creases; for each crease, loop through
        // the edges.
        int creaseIndexStart = 0;
        for (size_t i = 0; i < creaseLengths.size(); ++i) {
            int numEdges = creaseLengths[i] - 1;
            for (int j = 0; j < numEdges; ++j) {
                // Store the crease indices.
                ospCreaseIndices[2 * ospEdgeIndex + 0]
                       = creaseIndices[creaseIndexStart + j];
                ospCreaseIndices[2 * ospEdgeIndex + 1]
                       = creaseIndices[creaseIndexStart + j + 1];

                // Store the crease weight.
                ospCreaseWeights[ospEdgeIndex] = weightPerCrease
                       ? creaseWeights[i]
                       : creaseWeights[ospEdgeIndex];

                ospEdgeIndex++;
            }
            creaseIndexStart += creaseLengths[i];
        }

        mesh.setParam("edgeCrease.index", opp::CopiedData(ospCreaseIndices));
        mesh.setParam("edgeCrease.weight", opp::CopiedData(ospCreaseWeights));
    }

    if (numVertexCreases > 0) {
        opp::CopiedData vertex_crease_indices
               = opp::CopiedData(subdivTags.GetCornerIndices().cdata(),
                                 OSP_UINT, numVertexCreases);
        vertex_crease_indices.commit();
        mesh.setParam("vertexCrease.index", vertex_crease_indices);

        opp::CopiedData vertex_crease_weights
               = opp::CopiedData(subdivTags.GetCornerWeights().cdata(),
                                 OSP_FLOAT, numVertexCreases);
        vertex_crease_weights.commit();
        mesh.setParam("vertexCrease.weight", vertex_crease_weights);
    }
    mesh.commit();

    return mesh;
}

PXR_NAMESPACE_CLOSE_SCOPE
