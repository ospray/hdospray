// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

namespace opp = ospray::cpp;

#include <string>
#include <vector>

#define HDOSPRAY_DEFAULT_SPP_TO_CONVERGE 128
#define HDOSPRAY_DEFAULT_SPP 1
#define HDOSPRAY_DEFAULT_MAX_DEPTH 16
#define HDOSPRAY_DEFAULT_RR_START_DEPTH 1
#define HDOSPRAY_DEFAULT_MIN_CONTRIBUTION 0.01f
#define HDOSPRAY_DEFAULT_MAX_CONTRIBUTION 100.0f
#define HDOSPRAY_DEFAULT_INTERACTIVE_TARGET_FPS 30.0f
#define HDOSPRAY_DEFAULT_AO_RADIUS 0.5f
#define HDOSPRAY_DEFAULT_AO_SAMPLES 1
#define HDOSPRAY_DEFAULT_AO_INTENSITY 1.0f
#define HDOSPRAY_DEFAULT_TMP_ENABLED true
#define HDOSPRAY_DEFAULT_TMP_EXPOSURE 1.0f
#define HDOSPRAY_DEFAULT_TMP_CONTRAST 1.1759f
#define HDOSPRAY_DEFAULT_TMP_SHOULDER 0.9746f
#define HDOSPRAY_DEFAULT_TMP_HDRMAX 6.3704f
#define HDOSPRAY_DEFAULT_TMP_MIDIN 0.18f
#define HDOSPRAY_DEFAULT_TMP_MIDOUT 0.18f
#define HDOSPRAY_DEFAULT_TMP_ACESCOLOR false

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEBUG_CODES(OSP);

/// \class HdOSPRayConfig
///
///  Singletone class controlling settings for HdOSPRay.
///  settings can be controlled through USD settings or env vars of
///  the syntax HDOSPRAY_
///
class HdOSPRayConfig {
public:
    /// \brief Return the configuration singleton.
    static const HdOSPRayConfig& GetInstance();

    /// Samples per frame.  May be modified by interactive updates.
    ///
    /// Override with *HDOSPRAY_SAMPLES_PER_FRAME*.
    unsigned int samplesPerFrame { HDOSPRAY_DEFAULT_SPP };

    /// Override with *HDOSPRAY_SAMPLES_TO_CONVERGENCE*.
    unsigned int samplesToConvergence { HDOSPRAY_DEFAULT_SPP_TO_CONVERGE };

    /// Number of light samples
    /// A value of -1 means that all light are sampled.
    /// Override with *HDOSPRAY_LIGHT_SAMPLES*.
    unsigned int lightSamples;

    /// Number of AO samples.  Only affects scivis renderer.
    ///
    /// Override with *HDOSPRAY_AMBIENT_OCCLUSION_SAMPLES*.
    unsigned int ambientOcclusionSamples;

    ///  Whether OSPRay uses path tracing or scivis renderer.
    ///
    /// Override with *HDOSPRAY_USE_PATHTRACING*.
    bool usePathTracing;

    ///  Whether OSPRay uses denoiser
    ///
    /// Override with *HDOSPRAY_USE_DENOISER*.
    bool useDenoiser { true };

    /// The type of pixel filter used by OSPRay.  See OSPRay docs for list.
    ///
    /// Override with *HDOSPRAY_PIXELFILTER_TYPE*.
    OSPPixelFilterTypes pixelFilterType;

    /// Whether OSPRay should use a simple
    /// diffuse + phong lobe BRDF instead of the
    /// principled material.
    ///
    /// Override with *HDOSPRAY_USE_SIMPLE_MATERIAL*.
    bool useSimpleMaterial { false };

    /// Initialization arguments sent to OSPRay.
    ///  This can be used to set ospray configurations like mpi.
    ///
    /// Override with *HDOSPRAY_INIT_ARGS*.
    std::string initArgs;

    ///  Force Quad meshes
    ///
    /// Override with *HDOSPRAY_FORCE_QUADRANGULATE*.
    bool forceQuadrangulate;

    ///  Maximum ray depth
    ///
    /// Override with *HDOSPRAY_MAX_DEPTH*.
    int maxDepth { HDOSPRAY_DEFAULT_MAX_DEPTH };

    ///  Path depth at which Russian roulette is started being used
    ///
    /// Override with *HDOSPRAY_RR_START_DEPTH*.
    int rrStartDepth { HDOSPRAY_DEFAULT_RR_START_DEPTH };

    ///  Minimum intensity contribution
    ///
    /// Override with *HDOSPRAY_MIN_CONTRIBUTION*.
    float minContribution { HDOSPRAY_DEFAULT_MIN_CONTRIBUTION };

    ///  Maximum intensity contribution
    ///
    /// Override with *HDOSPRAY_MAX_CONTRIBUTION*.
    float maxContribution { HDOSPRAY_DEFAULT_MAX_CONTRIBUTION };

    /// Override with *HDOSPRAY_INTERACTIVE_TARGET_FPS*.
    float interactiveTargetFPS { HDOSPRAY_DEFAULT_INTERACTIVE_TARGET_FPS };

    ///  Ao rays maximum distance
    ///
    /// Override with *HDOSPRAY_AO_DISTANCE*.
    float aoRadius { HDOSPRAY_DEFAULT_AO_RADIUS };

    ///  The strength of the Ao effect.
    ///
    /// Override with *HDOSPRAY_AO_INTENSITY*.
    float aoIntensity { HDOSPRAY_DEFAULT_AO_INTENSITY };

    ///  Use an ambient light
    ///
    /// Override with *HDOSPRAY_AMBIENT_LIGHT*.
    bool ambientLight { false };

    ///  Use an eye light
    ///
    /// Override with *HDOSPRAY_STATIC_DIRECTIONAL_LIGHTS*.
    bool staticDirectionalLights { false };

    ///  Use an eye light
    ///
    /// Override with *HDOSPRAY_EYE_LIGHT*.
    bool eyeLight { false };

    ///  Use a key light
    ///
    /// Override with *HDOSPRAY_KEY_LIGHT*.
    bool keyLight { false };

    ///  Use a fill light
    ///
    /// Override with *HDOSPRAY_FILL_LIGHT*.
    bool fillLight { false };

    ///  Use a back light
    ///
    /// Override with *HDOSPRAY_BACK_LIGHT*.
    bool backLight { false };

    /// Tonemapper parameters
    ///
    /// Override with HDOSPRAY_TMP_*
    bool tmp_enabled { HDOSPRAY_DEFAULT_TMP_ENABLED };
    float tmp_exposure { HDOSPRAY_DEFAULT_TMP_EXPOSURE };
    float tmp_contrast { HDOSPRAY_DEFAULT_TMP_CONTRAST };
    float tmp_shoulder { HDOSPRAY_DEFAULT_TMP_SHOULDER };
    float tmp_hdrMax { HDOSPRAY_DEFAULT_TMP_HDRMAX };
    float tmp_midIn { HDOSPRAY_DEFAULT_TMP_MIDIN };
    float tmp_midOut { HDOSPRAY_DEFAULT_TMP_MIDOUT };
    bool tmp_acesColor { HDOSPRAY_DEFAULT_TMP_ACESCOLOR };

    // meshes populate global instances.  These are then committed by the
    // renderPass into a scene.
    std::vector<opp::Geometry> ospInstances;

private:
    // populates variables with env vars
    HdOSPRayConfig();
    ~HdOSPRayConfig() = default;

    HdOSPRayConfig(const HdOSPRayConfig&) = delete;
    HdOSPRayConfig& operator=(const HdOSPRayConfig&) = delete;

    friend class TfSingleton<HdOSPRayConfig>;
};