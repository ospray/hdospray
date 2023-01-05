// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "rendererPlugin.h"

#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include "config.h"
#include "renderDelegate.h"

// Register OSPRay plugin with USD
TF_REGISTRY_FUNCTION(TfType)
{
    HdRendererPluginRegistry::Define<HdOSPRayRendererPlugin>();
}

HdRenderDelegate*
HdOSPRayRendererPlugin::CreateRenderDelegate()
{
    // Check supported pxr version
#if PXR_MAJOR_VERSION != 0 || PXR_MINOR_VERSION < 20
    error This version of HdOSPRay is configured to built against USD
           v0 .20.x to v0 .22.x
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

        ospDeviceSetStatusCallback(
               device,
               [](void*, const char* msg) {
                   std::cerr << "OSPRAY STATUS: " << msg << std::endl;
               },
               nullptr);
        ospDeviceSetErrorCallback(
               device,
               [](void*, OSPError e, const char* msg) {
                   std::cerr << "OSPRAY ERROR [" << e << "]: " << msg
                             << std::endl;
               },
               nullptr);

        ospDeviceCommit(device);
    }
    if (ospGetCurrentDevice() == nullptr) {
        // bad user arguments, init with empty arguments instead
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
    return true;
}