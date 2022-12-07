// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/pxr.h>

#include "basisCurves.h"
#include "lights/light.h"
#include "mesh.h"

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

///
/// \class HdOSPRayRenderParam
///
/// Created by a render delegate to pass OSPRay specific state between 
/// different objects.
///
class HdOSPRayRenderParam final : public HdRenderParam {
public:
    HdOSPRayRenderParam(opp::Renderer renderer)
        : _renderer(renderer)
    {
    }
    virtual ~HdOSPRayRenderParam() = default;

    opp::Renderer GetOSPRayRenderer()
    {
        return _renderer;
    }

    void UpdateModelVersion()
    {
        _modelVersion++;
    }

    int GetModelVersion()
    {
        return _modelVersion.load();
    }

    void UpdateLightVersion()
    {
        _lightVersion++;
    }

    int GetLightVersion()
    {
        return _lightVersion.load();
    }

    // thread safe.  Lights added to scene and released by renderPass.
    void AddHdOSPRayLight(const SdfPath& id, const HdOSPRayLight* hdOsprayLight)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayLights[id] = hdOsprayLight;
        UpdateLightVersion();
    }

    void RemoveHdOSPRayLight(const SdfPath& id)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayLights.erase(id);
        UpdateLightVersion();
    }

    // not thread safe
    const std::unordered_map<SdfPath, const HdOSPRayLight*, SdfPath::Hash>&
    GetHdOSPRayLights()
    {
        return _hdOSPRayLights;
    }

    // thread safe.  Lights added to scene and released by renderPass.
    void AddHdOSPRayMesh(const HdOSPRayMesh* hdOsprayMesh)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayMeshes.push_back(hdOsprayMesh);
        UpdateModelVersion();
    }

    // thread safe.  Lights added to scene and released by renderPass.
    void AddHdOSPRayBasisCurves(const HdOSPRayBasisCurves* hdOsprayBasisCurves)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayBasisCurves.push_back(hdOsprayBasisCurves);
        UpdateModelVersion();
    }

    // not thread safe
    const std::vector<const HdOSPRayMesh*>& GetHdOSPRayMeshes()
    {
        return _hdOSPRayMeshes;
    }

    // not thread safe
    const std::vector<const HdOSPRayBasisCurves*>& GetHdOSPRayBasisCurves()
    {
        return _hdOSPRayBasisCurves;
    }

private:
    // mutex over ospray calls to the global model and global instances. OSPRay
    // is not thread safe
    std::mutex _ospMutex;
    // meshes populate global instances.  These are then committed by the
    // renderPass into a scene.
    std::unordered_map<SdfPath, const HdOSPRayLight*, SdfPath::Hash>
           _hdOSPRayLights;

    std::vector<const HdOSPRayMesh*> _hdOSPRayMeshes;
    std::vector<const HdOSPRayBasisCurves*> _hdOSPRayBasisCurves;

    opp::Renderer _renderer;
    /// A version counters for edits to scene (e.g., models or lights).
    std::atomic<int> _modelVersion { 1 };
    std::atomic<int> _lightVersion { 1 };
};