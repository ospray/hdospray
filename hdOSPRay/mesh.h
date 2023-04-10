// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/enums.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/vertexAdjacency.h>
#include <pxr/imaging/hd/vtBufferSource.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <mutex>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

class HdStDrawItem;
class HdOSPRayRenderParam;

/// \class HdOSPRayMesh
///
/// hdOSPRay RPrim object
/// - supports tri and quad meshes, as well as subd and curves.
///
class HdOSPRayMesh final : public HdMesh {
public:
    HF_MALLOC_TAG_NEW("new HdOSPRayMesh");

    ///   \param id scenegraph path
    ///   \param instancerId
    ///
    HdOSPRayMesh(SdfPath const& id, SdfPath const& instancerId = SdfPath());

    /// OSPRay resources released in Finalize
    virtual ~HdOSPRayMesh();

    virtual HdDirtyBits GetInitialDirtyBitsMask() const override;

    virtual void Finalize(HdRenderParam* renderParam) override;

    virtual void Sync(HdSceneDelegate* sceneDelegate,
                      HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
                      TfToken const& reprToken) override;

    /// Add generated instances from sync function to the instanceList for
    /// rendering
    void AddOSPInstances(std::vector<opp::Instance>& instanceList) const;

protected:
    bool _UseQuadIndices(const HdRenderIndex& renderIndex,
                         HdMeshTopology const& topology) const;

    virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    virtual void _InitRepr(TfToken const& reprToken,
                           HdDirtyBits* dirtyBits) override;

    void _UpdateDrawItemGeometricShader(HdSceneDelegate* sceneDelegate,
                                        HdStDrawItem* drawItem,
                                        const HdMeshReprDesc& desc,
                                        size_t drawItemIdForDesc);

private:
    // Populate the ospray geometry object based on scene data.
    void _PopulateOSPMesh(HdSceneDelegate* sceneDelegate,
                          opp::Renderer renderer, HdDirtyBits* dirtyBits,
                          HdMeshReprDesc const& desc,
                          HdOSPRayRenderParam* renderParam);

    void _UpdatePrimvarSources(HdSceneDelegate* sceneDelegate,
                               HdDirtyBits dirtyBits);

    opp::Geometry _CreateOSPRaySubdivMesh();
    opp::Geometry _CreateOSPRayMesh(const VtVec2fArray& texcoords,
                                    const VtVec3fArray& points,
                                    const VtVec3fArray& normals,
                                    const VtVec3fArray& colors, bool refined,
                                    bool useQuads);

    ///  \param meshUtil
    ///  \param useQuads
    ///  \param primvars input
    ///  \param computedPrimvars output
    ///  \param name token of primvar
    ///  \param interpolation facevarying, vertexvarying, uniform, or constant
    template <class type>
    void _ComputePrimvars(HdMeshUtil& meshUtil, bool useQuads, type& primvars,
                          type& computedPrimvars, TfToken name,
                          HdInterpolation interpolation)
    {
        if (useQuads) {
            if (interpolation == HdInterpolationFaceVarying) {
                VtValue buffValue = VtValue(primvars);
                HdVtBufferSource buffer(name, buffValue);
                VtValue quadPrimvar;

                auto success = meshUtil.ComputeQuadrangulatedFaceVaryingPrimvar(
                       buffer.GetData(), buffer.GetNumElements(),
                       buffer.GetTupleType().type, &quadPrimvar);
                if (success && quadPrimvar.IsHolding<type>()) {
                    computedPrimvars = quadPrimvar.Get<type>();
                } else {
                    TF_CODING_ERROR(
                           "ERROR: could not quadrangulate "
                           "face-varying data\n");
                }
            } else if (interpolation == HdInterpolationVarying
                       || interpolation == HdInterpolationVertex) {
                computedPrimvars = primvars;
            } else if (interpolation == HdInterpolationUniform) {
                computedPrimvars.resize(_quadIndices.size());
                for (size_t i = 0; i < computedPrimvars.size(); i++)
                    computedPrimvars[i] = primvars[i];
            } else if (interpolation == HdInterpolationConstant
                       && !primvars.empty()) {
                computedPrimvars.resize(1);
                computedPrimvars[0] = primvars[0];
            } else
                TF_CODING_ERROR("HdOSPRayMesh: unsupported interpolation mode");
        } else {
            if (interpolation == HdInterpolationFaceVarying) {
                VtValue buffValue = VtValue(primvars);
                HdVtBufferSource buffer(name, buffValue);
                VtValue triangulatedPrimvar;

                auto success = meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
                       buffer.GetData(), buffer.GetNumElements(),
                       buffer.GetTupleType().type, &triangulatedPrimvar);
                if (success && triangulatedPrimvar.IsHolding<type>()) {
                    computedPrimvars = triangulatedPrimvar.Get<type>();
                } else {
                    TF_CODING_ERROR(
                           "ERROR: could not triangulate "
                           "face-varying data\n");
                }
            } else if (interpolation == HdInterpolationVarying
                       || interpolation == HdInterpolationVertex) {
                computedPrimvars = primvars;
            } else if (interpolation == HdInterpolationUniform) {
                computedPrimvars.resize(_triangulatedIndices.size());
                for (size_t i = 0; i < computedPrimvars.size(); i++)
                    computedPrimvars[i] = primvars
                           [HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(
                                  _trianglePrimitiveParams[i])];
            } else if (interpolation == HdInterpolationConstant
                       && !primvars.empty()) {
                computedPrimvars.resize(1);
                computedPrimvars[0] = primvars[0];
            } else if (interpolation == HdInterpolationInstance) {
                TF_WARN("HdOSPRayMesh: unsupported interpolation mode HdInterpolationInstance");
            } else {
                TF_WARN("HdOSPRayMesh: unknown interpolation mode");
            }
        }
    }

    bool _populated { false };

    opp::Geometry _ospMesh;
    opp::GeometricModel* _geometricModel;
    std::vector<opp::GeometricModel> _geomSubsetModels;
    // Each instance of the mesh in the top-level scene is stored in
    // _ospInstances. This gets queried by the renderpass.
    std::vector<opp::Instance> _ospInstances;

    HdMeshUtil* _meshUtil { nullptr };
    HdMeshTopology _topology;
    GfMatrix4f _transform;
    VtVec3fArray _points;
    VtVec2fArray _texcoords;
    VtVec2fArray _computedTexcoords; // triangulated
    VtVec3fArray _normals;
    VtVec3fArray _computedNormals; // triangulated
    VtVec3fArray _colors;
    VtVec3fArray _computedColors; // triangulated
    GfVec4f _singleColor { .5f, .5f, .5f, 1.f };
    HdInterpolation _texcoordsInterpolation {HdInterpolationVertex};
    HdInterpolation _colorsInterpolation {HdInterpolationVertex};
    HdInterpolation _normalsInterpolation {HdInterpolationVarying};
    TfToken _texcoordsPrimVarName;
    TfToken _colorsPrimVarName;
    TfToken _normalsPrimVarName;

    VtVec3iArray _triangulatedIndices;
    VtIntArray _trianglePrimitiveParams;

#if HD_API_VERSION < 44
    VtVec4iArray _quadIndices;
#else
    VtIntArray _quadIndices;
#endif
#if HD_API_VERSION < 36
    VtVec2iArray _quadPrimitiveParams;
#else
    VtIntArray _quadPrimitiveParams;
#endif

    Hd_VertexAdjacency _adjacency;
    bool _adjacencyValid;
    bool _normalsValid;

    // Draw styles.
    bool _refined;
    bool _smoothNormals { false };
    bool _doubleSided { false };
    HdCullStyle _cullStyle;
    int _tessellationRate { 32 };
    std::mutex _mutex;

    // This class does not support copying.
    HdOSPRayMesh(const HdOSPRayMesh&) = delete;
    HdOSPRayMesh& operator=(const HdOSPRayMesh&) = delete;
};