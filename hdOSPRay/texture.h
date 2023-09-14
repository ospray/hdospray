// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <string>
#include <stdint.h>

namespace opp = ospray::cpp;

PXR_NAMESPACE_USING_DIRECTIVE

OSPTextureFormat osprayTextureFormat(int depth, int channels,
                                     bool preferLinear = false);

opp::Texture LoadPtexTexture(std::string file);

/// @brief  Load pxr Hio Texture
/// @param filename
/// @param nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
std::pair<opp::Texture, std::shared_ptr<uint8_t[]> >
LoadHioTexture2D(const std::string file, const std::string channels = "",
                  bool nearestFilter = false, bool complement = false);

/// @brief
/// @param filename
/// @param number of udim tile columns
/// @param number of udim tile rows
/// @param use nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
std::pair<opp::Texture, std::shared_ptr<uint8_t[]> > LoadUDIMTexture2D(std::string file, int& numX,
                                                 int& numY,
                                                 bool nearestFilter = false,
                                                 bool complement = false);