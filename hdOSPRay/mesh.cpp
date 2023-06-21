// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "mesh.h"
#include "config.h"
#include "context.h"
#include "instancer.h"
#include "material.h"
#include "renderParam.h"
#include "renderPass.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/pxOsd/tokens.h>

#include <rkcommon/math/AffineSpace.h>

using namespace rkcommon::math;

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayTokens,
    (st)
);
// clang-format on

HdOSPRayMesh::HdOSPRayMesh(SdfPath const& id, SdfPath const& instancerId)
#if HD_API_VERSION < 36
    : HdMesh(id, instancerId)
#else
    : HdMesh(id)
#endif
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
    delete _meshUtil;
}

void
HdOSPRayMesh::Finalize(HdRenderParam* renderParam)
{
}

HdDirtyBits
HdOSPRayMesh::GetInitialDirtyBitsMask() const
{
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
    if (topology.GetScheme() == PxOsdOpenSubdivTokens->loop)
        return false;

    const HdOSPRayMaterial* material = static_cast<const HdOSPRayMaterial*>(
           renderIndex.GetSprim(HdPrimTypeTokens->material, GetMaterialId()));
    if (material && material->HasPtex())
        return true;

    return _IsEnabledForceQuadrangulate();
}

void
HdOSPRayMesh::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
                   HdDirtyBits* dirtyBits, TfToken const& reprToken)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const HdMeshReprDesc& desc = descs[0];

    HdOSPRayRenderParam* ospRenderParam
           = static_cast<HdOSPRayRenderParam*>(renderParam);
    opp::Renderer renderer = ospRenderParam->GetOSPRayRenderer();

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
#if HD_API_VERSION < 37
        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(),
                       sceneDelegate->GetMaterialId(GetId()));
#else
        SetMaterialId(sceneDelegate->GetMaterialId(GetId()));
#endif
    }

    // Create ospray mesh
    _PopulateOSPMesh(sceneDelegate, renderer, dirtyBits, desc, ospRenderParam);

    if (*dirtyBits & HdChangeTracker::DirtyTopology) {
        // TODO: need to update material when topology has changed?
    }
}

void
HdOSPRayMesh::_UpdatePrimvarSources(HdSceneDelegate* sceneDelegate,
                                    HdDirtyBits dirtyBits)
{
    HD_TRACE_FUNCTION();
    SdfPath const& id = GetId();

    TF_VERIFY(sceneDelegate);

    HdPrimvarDescriptorVector primvars;
    for (size_t i = 0; i < HdInterpolationCount; ++i) {
        HdInterpolation interp = static_cast<HdInterpolation>(i);
        primvars = GetPrimvarDescriptors(sceneDelegate, interp);
        for (HdPrimvarDescriptor const& pv : primvars) {

            if (pv.name == HdTokens->points)
                continue;

            auto value = sceneDelegate->Get(id, pv.name);

            if ((pv.role == HdPrimvarRoleTokens->textureCoordinate
                 || pv.name == HdOSPRayTokens->st)
                && HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                   HdOSPRayTokens->st)) {
                if (value.IsHolding<VtVec2fArray>()) {
                    _texcoords = value.Get<VtVec2fArray>();
                    _texcoordsPrimVarName = pv.name;
                    _texcoordsInterpolation = interp;
                }
            } else if (pv.name == HdTokens->normals) {
                if (value.IsHolding<VtVec3fArray>()) {
                    _normals = value.Get<VtVec3fArray>();
                    _normalsPrimVarName = pv.name;
                    _normalsInterpolation = interp;
                }
            } else if (pv.name == HdTokens->displayColor
                       && HdChangeTracker::IsPrimvarDirty(
                              dirtyBits, id, HdTokens->displayColor)) {
                _colorsInterpolation = interp;
                if (value.IsHolding<VtVec3fArray>()) {
                    _colorsPrimVarName = pv.name;
                    _colors = value.Get<VtVec3fArray>();
                }
            }
            // TODO: check display opacity
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

    // do not subd wireframes
    bool doRefine = (desc.geomStyle == HdMeshGeomStyleSurf);

    // subdiv or triangle/quads?
    doRefine
           = doRefine && (_topology.GetScheme() != PxOsdOpenSubdivTokens->none);

    // if subdiv level is 0, triangulate
    doRefine = doRefine && (_topology.GetRefineLevel() > 0);

    _smoothNormals = !desc.flatShadingEnabled;

    _smoothNormals = _smoothNormals
           && ((_topology.GetScheme() != PxOsdOpenSubdivTokens->none));

    if (_meshUtil)
        delete _meshUtil;
    _meshUtil = new HdMeshUtil(&_topology, GetId());

    const HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    bool useQuads = _UseQuadIndices(renderIndex, _topology);

    if (HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id)
        && _topology.GetRefineLevel() > 0) {
        _topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }

    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        if (doRefine) { // set subdiv rate
            _tessellationRate = (1 << _topology.GetRefineLevel());
            if (_tessellationRate == 1) {
                _tessellationRate++;
            }
        }
    }

    bool newMesh = false;
    // if topology dirty, update topology and build new mesh
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)
        || doRefine != _refined) {
        newMesh = true;

        _normalsValid = false;

        PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
        int refineLevel = _topology.GetRefineLevel();
        _topology = HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _topology.SetSubdivTags(subdivTags);
        _adjacencyValid = false;

        if (doRefine && !_points.empty()) {
            _ospMesh = _CreateOSPRaySubdivMesh();
        }
        _refined = doRefine;
    }

    _normalsValid = false;
    if (_smoothNormals && !_adjacencyValid) {
        _adjacency.BuildAdjacencyTable(&_topology);
        _adjacencyValid = true;
        // If we rebuilt the adjacency table, force a rebuild of normals.
        _normalsValid = false;
    }
    // calculate new smooth normals
    if (_normals.empty() && _smoothNormals && !_normalsValid && !doRefine) {
        _normals = Hd_SmoothNormals::ComputeSmoothNormals(
               &_adjacency, _points.size(), _points.cdata());
        _normalsValid = true;
    }

    if (newMesh
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id,
                                           HdOSPRayTokens->st)) {
        newMesh = true;

        if (!_refined) {
            if (useQuads) {
                _meshUtil->ComputeQuadIndices(&_quadIndices,
                                              &_quadPrimitiveParams);
            } else {
                _meshUtil->ComputeTriangleIndices(&_triangulatedIndices,
                                                  &_trianglePrimitiveParams);
            }

            if ((_quadIndices.empty() && _triangulatedIndices.empty())
                || _points.empty())
                return;

            if (!_colors.empty()) {
                _ComputePrimvars<VtVec3fArray>(
                       *_meshUtil, useQuads, _colors, _computedColors,
                       _colorsPrimVarName, _colorsInterpolation);
            }
            if (!_normals.empty()) {
                _ComputePrimvars<VtVec3fArray>(
                       *_meshUtil, useQuads, _normals, _computedNormals,
                       _normalsPrimVarName, _normalsInterpolation);
            }
            if (!_texcoords.empty()) {
                _ComputePrimvars<VtVec2fArray>(
                       *_meshUtil, useQuads, _texcoords, _computedTexcoords,
                       _texcoordsPrimVarName, _texcoordsInterpolation);
            }

            _ospMesh = _CreateOSPRayMesh(_computedTexcoords, _points,
                                         _computedNormals, _computedColors,
                                         _refined, useQuads);
        }

        if (!_normals.empty()) {
            VtVec3fArray& normals = _normals;
            if (!_computedNormals.empty())
                normals = _computedNormals;
            opp::SharedData normalsData = opp::SharedData(
                   normals.cdata(), OSP_VEC3F, normals.size());
            normalsData.commit();
            if (_normalsInterpolation == HdInterpolationFaceVarying) {
                _ospMesh.setParam("normal", normalsData);
            } else if ((_normalsInterpolation == HdInterpolationVarying)
                     || (_normalsInterpolation == HdInterpolationVertex)) {
                _ospMesh.setParam("vertex.normal", normalsData);
            } else {
                TF_DEBUG_MSG(OSP, "osp::mesh unsupported normal interpolation mode");
            }
        }

        if (!_colors.empty()) {
            // TODO: add back in opacities
            VtVec3fArray& colors = _colors;
            if (!_computedColors.empty())
                colors = _computedColors;
            opp::SharedData colorsData
                   = opp::SharedData(colors.cdata(), OSP_VEC3F, colors.size());
            colorsData.commit();
            if (_colorsInterpolation == HdInterpolationFaceVarying)
                _ospMesh.setParam("color", colorsData);
            else if ((_colorsInterpolation == HdInterpolationVarying)
                     || (_colorsInterpolation == HdInterpolationVertex))
                _ospMesh.setParam("vertex.color", colorsData);
        }

        if (_texcoords.size() > 1) {
            VtVec2fArray& texcoords = _texcoords;
            if (!_computedTexcoords.empty())
                texcoords = _computedTexcoords;
            opp::SharedData texcoordsData = opp::SharedData(
                   texcoords.cdata(), OSP_VEC2F, texcoords.size());
            texcoordsData.commit();
            if (_texcoordsInterpolation == HdInterpolationFaceVarying)
                _ospMesh.setParam("texcoord", texcoordsData);
            else if (_texcoordsInterpolation == HdInterpolationVertex
                     || _texcoordsInterpolation == HdInterpolationVarying) {
                _ospMesh.setParam("vertex.texcoord", texcoordsData);
            } else {
                TF_DEBUG_MSG(OSP, "unsupported texcoord interpolation mode");
            }
        }

        _ospMesh.commit();

        const HdOSPRayMaterial* subsetMaterial = nullptr;

        for (auto subset : _topology.GetGeomSubsets()) {
            if (!TF_VERIFY(subset.type == HdGeomSubset::TypeFaceSet))
                continue;

            const HdOSPRayMaterial* material
                   = static_cast<const HdOSPRayMaterial*>(renderIndex.GetSprim(
                          HdPrimTypeTokens->material, subset.materialId));
            subsetMaterial = material;
        }

        const HdOSPRayMaterial* material = nullptr;
        if (subsetMaterial)
            material = subsetMaterial;
        else
            material
                   = static_cast<const HdOSPRayMaterial*>(renderIndex.GetSprim(
                          HdPrimTypeTokens->material, GetMaterialId()));

        opp::Material ospMaterial = nullptr;

        if (material && material->GetOSPRayMaterial()) {
            ospMaterial = material->GetOSPRayMaterial();
        } else {
            ospMaterial = HdOSPRayMaterial::CreateDefaultMaterial(_singleColor);
        }

        // Create OSPRay Mesh
        if (_geometricModel)
            delete _geometricModel;
        _geometricModel = new opp::GeometricModel(_ospMesh);

        _geometricModel->setParam("material", ospMaterial);
        _geometricModel->setParam("id", (unsigned int)GetPrimId());
        _ospMesh.commit();
        if (_colorsInterpolation == HdInterpolationUniform
            && !_computedColors.empty()) {
            std::vector<vec4f> colors(_computedColors.size());
            for (int i = 0; i < _computedColors.size(); i++) {
                const auto& c = _computedColors[i];
                colors[i] = vec4f(c[0], c[1], c[2], 1.f);
            }
            _geometricModel->setParam("color", opp::CopiedData(colors));
        }
        if (_colorsInterpolation == HdInterpolationConstant
            && !_computedColors.empty()) {
            _geometricModel->setParam(
                   "color",
                   vec4f(_colors[0][0], _colors[0][1], _colors[0][2], 1.f));
        }

        _geometricModel->commit();

        renderParam->UpdateModelVersion();
    }

#if HD_API_VERSION < 36
#else
    _UpdateInstancer(sceneDelegate, dirtyBits);
    HdInstancer::_SyncInstancerAndParents(sceneDelegate->GetRenderIndex(),
                                          GetInstancerId());
#endif

    if (HdChangeTracker::IsInstancerDirty(*dirtyBits, id) || isTransformDirty) {
        if (!GetInstancerId().IsEmpty()) {
            //TODO: reuse instances for instancer?
            _ospInstances.clear();
            HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
            HdInstancer* instancer = renderIndex.GetInstancer(GetInstancerId());
            VtMatrix4dArray transforms
                   = static_cast<HdOSPRayInstancer*>(instancer)
                            ->ComputeInstanceTransforms(GetId());

            size_t newSize = transforms.size();

            opp::Group group;
            if (_geometricModel)
                group.setParam("geometry", opp::CopiedData(*_geometricModel));
            group.commit();

            _ospInstances.reserve(newSize);
            for (size_t i = 0; i < newSize; i++) {
                opp::Instance instance(group);

                GfMatrix4f matf = _transform * GfMatrix4f(transforms[i]);
                float* xfmf = matf.GetArray();
                affine3f xfm(vec3f(xfmf[0], xfmf[1], xfmf[2]),
                             vec3f(xfmf[4], xfmf[5], xfmf[6]),
                             vec3f(xfmf[8], xfmf[9], xfmf[10]),
                             vec3f(xfmf[12], xfmf[13], xfmf[14]));
                instance.setParam("transform", xfm);
                instance.setParam("id", (unsigned int)i);
                instance.commit();
                _ospInstances.push_back(instance);
            }
        } else {
            if (newMesh)
                _ospInstances.clear();
            opp::Group group;
            opp::Instance instance;
            if (newMesh)
                instance = opp::Instance(group);
            else
                instance = _ospInstances.back();
            GfMatrix4f matf = _transform;
            float* xfmf = matf.GetArray();
            affine3f xfm(vec3f(xfmf[0], xfmf[1], xfmf[2]),
                         vec3f(xfmf[4], xfmf[5], xfmf[6]),
                         vec3f(xfmf[8], xfmf[9], xfmf[10]),
                         vec3f(xfmf[12], xfmf[13], xfmf[14]));
            instance.setParam("transform", xfm);
            instance.setParam("id", (unsigned int)0);
            instance.commit();

            if (newMesh) {
                if (_geomSubsetModels.size()) {
                    group.setParam("geometry", opp::CopiedData(_geomSubsetModels));
                } else {
                    if (_geometricModel)
                        group.setParam("geometry", opp::CopiedData(*_geometricModel));
                }
                group.commit();
                _ospInstances.push_back(instance);
            }
        }
        renderParam->UpdateModelVersion();
    }
    if (!_populated) {
        renderParam->AddHdOSPRayMesh(this);
        _populated = true;
    }

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
    HdCullStyle cullStyle = desc.cullStyle;

    if (cullStyle == HdCullStyleDontCare) {
        cullStyle = _cullStyle;
    }
}

opp::Geometry
HdOSPRayMesh::_CreateOSPRayMesh(const VtVec2fArray& texcoords,
                                const VtVec3fArray& points,
                                const VtVec3fArray& normals,
                                const VtVec3fArray& colors, bool refined,
                                bool useQuads)
{
    opp::Geometry ospMesh = opp::Geometry("mesh");
    if (!_refined) {
        if (useQuads) {
            size_t numQuads = _quadIndices.size() / 4;
            #if HD_API_VERSION < 44
                        numQuads = _quadIndices.size();
            #endif
            opp::SharedData indices = opp::SharedData(
                   _quadIndices.cdata(), OSP_VEC4UI, numQuads);
            indices.commit();
            ospMesh.setParam("index", indices);

        } else { // triangles
            opp::SharedData indices
                   = opp::SharedData(_triangulatedIndices.cdata(), OSP_VEC3UI,
                                     _triangulatedIndices.size());
            indices.commit();
            ospMesh.setParam("index", indices);
        }
    }

    opp::SharedData verticesData
           = opp::SharedData(points.cdata(), OSP_VEC3F, points.size());
    verticesData.commit();
    ospMesh.setParam("vertex.position", verticesData);

    ospMesh.setParam("id", (unsigned int)GetPrimId());
    return ospMesh;
}

opp::Geometry
HdOSPRayMesh::_CreateOSPRaySubdivMesh()
{
    const PxOsdSubdivTags& subdivTags = _topology.GetSubdivTags();

    VtIntArray const creaseLengths = subdivTags.GetCreaseLengths();
    int numEdgeCreases = 0;
    for (size_t i = 0; i < creaseLengths.size(); ++i) {
        numEdgeCreases += creaseLengths[i] - 1;
    }

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
    if (numFaceVertices > 0) {
        opp::SharedData faces
               = opp::SharedData(_topology.GetFaceVertexCounts().data(),
                                 OSP_UINT, numFaceVertices);
        faces.commit();
        mesh.setParam("face", faces);
    }
    if (numIndices > 0) {
        opp::SharedData indices = opp::SharedData(
               _topology.GetFaceVertexIndices().data(), OSP_UINT, numIndices);
        indices.commit();
        mesh.setParam("index", indices);
    }

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

    mesh.setParam("level", _tessellationRate);

    if (numEdgeCreases > 0) {
        std::vector<unsigned int> ospCreaseIndices(numEdgeCreases * 2);
        std::vector<float> ospCreaseWeights(numEdgeCreases);
        int ospEdgeIndex = 0;

        VtIntArray const creaseIndices = subdivTags.GetCreaseIndices();
        VtFloatArray const creaseWeights = subdivTags.GetCreaseWeights();

        bool weightPerCrease = (creaseWeights.size() == creaseLengths.size());

        int creaseIndexStart = 0;
        for (size_t i = 0; i < creaseLengths.size(); ++i) {
            int numEdges = creaseLengths[i] - 1;
            for (int j = 0; j < numEdges; ++j) {
                ospCreaseIndices[2 * ospEdgeIndex]
                       = creaseIndices[creaseIndexStart + j];
                ospCreaseIndices[2 * ospEdgeIndex + 1]
                       = creaseIndices[creaseIndexStart + j + 1];

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
    mesh.setParam("id", (unsigned int)GetPrimId());

    return mesh;
}