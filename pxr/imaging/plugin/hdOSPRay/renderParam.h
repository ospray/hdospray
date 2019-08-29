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

#include "ospray/ospray.h"

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
    HdOSPRayRenderParam(OSPRenderer renderer, std::atomic<int>* sceneVersion)
        : _renderer(renderer)
        , _sceneVersion(sceneVersion)
    {
    }
    virtual ~HdOSPRayRenderParam() = default;

    OSPRenderer GetOSPRayRenderer()
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

    // thread safe.  Instances added to scene and released by renderPass.
    void AddInstance(const OSPGeometry instance)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _ospInstances.emplace_back(instance);
    }

    // thread safe.  Instances added to scene and released by renderPass.
    void AddInstances(const std::vector<OSPGeometry>& instances)
    {
        std::lock_guard<std::mutex> lock(_ospMutex);
        _ospInstances.insert(_ospInstances.end(), instances.begin(),
                             instances.end());
    }

    void ClearInstances()
    {
        _ospInstances.resize(0);
    }

    // not thread safe
    std::vector<OSPGeometry>& GetInstances()
    {
        return _ospInstances;
    }

private:
    // mutex over ospray calls to the global model and global instances. OSPRay
    // is not thread safe
    std::mutex _ospMutex;
    // meshes populate global instances.  These are then committed by the
    // renderPass into a scene.
    std::vector<OSPGeometry> _ospInstances;
    OSPRenderer _renderer;
    /// A version counter for edits to _scene.
    std::atomic<int>* _sceneVersion;
    // version of osp model.  Used for tracking image changes
    std::atomic<int> _modelVersion { 1 };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDOSPRAY_RENDER_PARAM_H
