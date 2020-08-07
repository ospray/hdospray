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
#include "pxr/imaging/hdOSPRay/rendererPlugin.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"
#include "pxr/imaging/hdOSPRay/config.h"
#include "pxr/imaging/hdOSPRay/renderDelegate.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

// Register the OSPRay plugin with the renderer plugin system.
TF_REGISTRY_FUNCTION(TfType)
{
    HdRendererPluginRegistry::Define<HdOSPRayRendererPlugin>();
}

HdRenderDelegate*
HdOSPRayRendererPlugin::CreateRenderDelegate()
{
    // Check plugin against pxr version
#if PXR_MAJOR_VERSION != 0 || PXR_MINOR_VERSION != 20
    error This version of HdOSPRay is configured to built against USD v0 .20.x
#endif

           int ac
           = 1;
    std::string initArgs = HdOSPRayConfig::GetInstance().initArgs;
    std::stringstream ss(initArgs);
    std::string arg;
    std::vector<std::string> args;
    while (ss >> arg) {
        args.push_back(arg);
    }
    ac = static_cast<int>(args.size() + 1);
    const char** av = new const char*[ac];
    av[0] = "ospray";
    for (int i = 1; i < ac; i++) {
        av[i] = args[i - 1].c_str();
    }
    int init_error = ospInit(&ac, av);
    if (init_error != OSP_NO_ERROR) {
        std::cerr << "FATAL ERROR DURING INITIALIZATION!" << std::endl;
    } else {
        auto device = ospGetCurrentDevice();
        if (device == nullptr) {
            std::cerr << "FATAL ERROR DURING GETTING CURRENT DEVICE!"
                      << std::endl;
        }

        ospDeviceSetStatusFunc(device,
                               [](const char* msg) { std::cout << msg; });
        ospDeviceSetErrorFunc(device, [](OSPError e, const char* msg) {
            std::cerr << "OSPRAY ERROR [" << e << "]: " << msg << std::endl;
        });

        ospDeviceCommit(device);
    }
    if (ospGetCurrentDevice() == nullptr) {
        // user most likely specified bad arguments, retry without them
        ac = 1;
        ospInit(&ac, av);
    }
    delete[] av;

    return new HdOSPRayRenderDelegate();
}

void
HdOSPRayRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate)
{
    delete renderDelegate;
    ospShutdown();
}

bool
HdOSPRayRendererPlugin::IsSupported() const
{
    // Nothing more to check for now, we assume if the plugin loads correctly
    // it is supported.
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
