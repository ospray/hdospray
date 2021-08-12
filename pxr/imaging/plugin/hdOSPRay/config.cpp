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
#include "config.h"

#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/instantiateSingleton.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

// Instantiate the config singleton.
TF_INSTANTIATE_SINGLETON(HdOSPRayConfig);

// Each configuration variable has an associated environment variable.
// The environment variable macro takes the variable name, a default value,
// and a description...
// clang-format off
TF_DEFINE_ENV_SETTING(HDOSPRAY_SAMPLES_PER_FRAME, HDOSPRAY_DEFAULT_SPP,
        "Raytraced samples per pixel per frame (must be >= 1)");

TF_DEFINE_ENV_SETTING(HDOSPRAY_SAMPLES_TO_CONVERGENCE, HDOSPRAY_DEFAULT_SPP_TO_CONVERGE,
        "Samples per pixel before we stop rendering (must be >= 1)");

TF_DEFINE_ENV_SETTING(HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES, HDOSPRAY_DEFAULT_AO_SAMPLES,
        "Ambient occlusion samples per camera ray (must be >= 0; a value of 0 disables ambient occlusion)");

TF_DEFINE_ENV_SETTING(HDOSPRAY_LIGHT_SAMPLES, 1,
        "Light samples at every path intersection. A value of -1 means that all light are sampled.");

TF_DEFINE_ENV_SETTING(HDOSPRAY_CAMERA_LIGHT_INTENSITY, 300,
        "Intensity of the camera light, specified as a percentage of <1,1,1>.");

TF_DEFINE_ENV_SETTING(HDOSPRAY_PRINT_CONFIGURATION, 0,
        "Should HdOSPRay print configuration on startup? (values > 0 are true)");

TF_DEFINE_ENV_SETTING(HDOSPRAY_USE_PATH_TRACING, 1,
        "Should HdOSPRay use path tracing");

TF_DEFINE_ENV_SETTING(HDOSPRAY_MAX_PATH_DEPTH, HDOSPRAY_DEFAULT_MAX_DEPTH,
        "Maximum path tracing depth.");

TF_DEFINE_ENV_SETTING(HDOSPRAY_RR_START_DEPTH, HDOSPRAY_DEFAULT_RR_START_DEPTH,
        "Path depth Russian roulette starts.");

TF_DEFINE_ENV_SETTING(HDOSPRAY_INIT_ARGS, "",
        "Initialization arguments sent to OSPRay");

TF_DEFINE_ENV_SETTING(HDOSPRAY_USE_SIMPLE_MATERIAL, 0,
        "If OSPRay uses a simple diffuse + phong based material instead of the principled material");

TF_DEFINE_ENV_SETTING(HDOSPRAY_USE_DENOISER, 1,
        "OSPRay uses denoiser");

TF_DEFINE_ENV_SETTING(HDOSPRAY_PIXELFILTER_TYPE, OSPPixelFilterTypes::OSP_PIXELFILTER_GAUSS,
        "The type of pixel filter used by OSPRay: 0 (point), 1 (box), 2 (gauss), 3 (mitchell), and 4 (blackmanHarris)");

TF_DEFINE_ENV_SETTING(HDOSPRAY_FORCE_QUADRANGULATE, 0,
        "OSPRay force Quadrangulate meshes for debug");

HdOSPRayConfig::HdOSPRayConfig()
{
    // Read in values from the environment, clamping them to valid ranges.
    samplesPerFrame = std::max(-1,
            TfGetEnvSetting(HDOSPRAY_SAMPLES_PER_FRAME));
    samplesToConvergence = std::max(1,
            TfGetEnvSetting(HDOSPRAY_SAMPLES_TO_CONVERGENCE));
    ambientOcclusionSamples = std::max(0,
            TfGetEnvSetting(HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES));
    lightSamples = std::max(-1,
            TfGetEnvSetting(HDOSPRAY_LIGHT_SAMPLES));

    cameraLightIntensity = (std::max(100,
            TfGetEnvSetting(HDOSPRAY_CAMERA_LIGHT_INTENSITY)) / 100.0f);
    usePathTracing =TfGetEnvSetting(HDOSPRAY_USE_PATH_TRACING);
    initArgs =TfGetEnvSetting(HDOSPRAY_INIT_ARGS);
    useDenoiser = bool(TfGetEnvSetting(HDOSPRAY_USE_DENOISER) == 1);
    pixelFilterType = (OSPPixelFilterTypes) TfGetEnvSetting(HDOSPRAY_PIXELFILTER_TYPE);
    forceQuadrangulate = TfGetEnvSetting(HDOSPRAY_FORCE_QUADRANGULATE);
    maxDepth = TfGetEnvSetting(HDOSPRAY_MAX_PATH_DEPTH);
    useSimpleMaterial = TfGetEnvSetting(HDOSPRAY_USE_SIMPLE_MATERIAL);

    if (TfGetEnvSetting(HDOSPRAY_PRINT_CONFIGURATION) > 0) {
        std::cout
            << "HdOSPRay Configuration: \n"
            << "  samplesPerFrame            = "
            <<    samplesPerFrame         << "\n"
            << "  samplesToConvergence       = "
            <<    samplesToConvergence    << "\n"
            << "  ambientOcclusionSamples    = "
            <<    ambientOcclusionSamples << "\n"
            << "  cameraLightIntensity      = "
            <<    cameraLightIntensity   << "\n"
            << "  minContribution      = "
            <<    minContribution   << "\n"
            << "  maxContribution      = "
            <<    maxContribution   << "\n"
            << "  initArgs                  = "
            <<    initArgs   << "\n"
            ;
    }
}

/*static*/
const HdOSPRayConfig&
HdOSPRayConfig::GetInstance()
{
    return TfSingleton<HdOSPRayConfig>::GetInstance();
}

PXR_NAMESPACE_CLOSE_SCOPE
