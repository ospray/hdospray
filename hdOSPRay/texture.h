// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <stdint.h>
#include <string>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

struct HdOSPRayTexture {
public:
    HdOSPRayTexture(opp::Texture texture = nullptr,
                    std::shared_ptr<uint8_t> data_ = nullptr)
        : ospTexture(std::move(texture))
        , data(std::move(data_))
    {
    }
    ~HdOSPRayTexture() = default;
    std::string file;
    enum class WrapType { NONE, BLACK, CLAMP, REPEAT, MIRROR };
    WrapType wrapS {WrapType::CLAMP};
    WrapType wrapT {WrapType::CLAMP};
    GfVec4f scale { 1.0f };
    GfVec2f xfm_translation { 0.f, 0.f };
    GfVec2f xfm_scale { 1.f, 1.f };
    float xfm_rotation { 0.f };
    bool hasXfm { false };
    enum class ColorType { NONE, RGBA, RGB, R, G, B, A };
    ColorType type {ColorType::NONE};
    opp::Texture ospTexture { nullptr };
    bool isPtex { false };
    std::shared_ptr<uint8_t> data; // should be uint8_t[], but to support older
                                   // compilers use custom delete
};

OSPTextureFormat osprayTextureFormat(int depth, int channels,
                                     bool preferLinear = false);

opp::Texture LoadPtexTexture(std::string file);

/// @brief  Load pxr Hio Texture
/// @param filename
/// @param nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
HdOSPRayTexture LoadHioTexture2D(const std::string file,
                                 const std::string channels = "",
                                 bool nearestFilter = false,
                                 bool complement = false);

/// @brief
/// @param filename
/// @param number of udim tile columns
/// @param number of udim tile rows
/// @param use nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
HdOSPRayTexture LoadUDIMTexture2D(std::string file, int& numX, int& numY,
                                  bool nearestFilter = false,
                                  bool complement = false);