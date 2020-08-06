//
// Copyright 2018 Intel
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
#ifndef HDOSPRAY_RENDER_PARAM_H
#define HDOSPRAY_RENDER_PARAM_H

#include "pxr/imaging/hd/renderDelegate.h"
#include "pxr/pxr.h"

#include "mesh.h"
#include "lights/light.h"

#include "ospray/ospray_cpp.h"

namespace opp = ospray::cpp;

PXR_NAMESPACE_OPEN_SCOPE

///
/// \class HdOSPRayRenderParam
///
/// The render delegate can create an object of type HdRenderParam, to pass
/// to each prim during Sync(). HdOSPRay uses this class to pass
/// OSPRay state around.
///
class HdOSPRayRenderParam final : public HdRenderParam {
public:
    HdOSPRayRenderParam(opp::Renderer renderer, std::atomic<int>* sceneVersion)
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
    void AddHdOSPRayLight(const SdfPath &id, const HdOSPRayLight* hdOsprayLight)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayLights[id] = hdOsprayLight;
        UpdateLightVersion();
    }

    void RemoveHdOSPRayLight(const SdfPath &id)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _hdOSPRayLights.erase(id);
        UpdateLightVersion();
    }

    // not thread safe
    const std::unordered_map<SdfPath, const HdOSPRayLight*, SdfPath::Hash> &GetHdOSPRayLights()
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

    // not thread safe
    const std::vector<const HdOSPRayMesh*> &GetHdOSPRayMeshes()
    {
        return _hdOSPRayMeshes;
    }

private:
    // mutex over ospray calls to the global model and global instances. OSPRay
    // is not thread safe
    std::mutex _ospMutex;
    // meshes populate global instances.  These are then committed by the
    // renderPass into a scene.
    std::unordered_map<SdfPath, const HdOSPRayLight*, SdfPath::Hash> _hdOSPRayLights;

    std::vector<const HdOSPRayMesh*> _hdOSPRayMeshes;

    opp::Renderer _renderer;
    /// A version counters for edits to scene (e.g., models or lights).
    std::atomic<int> _modelVersion { 1 };

    std::atomic<int> _lightVersion { 1 };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_RENDER_PARAM_H
