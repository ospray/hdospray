// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/pxr.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

namespace opp = ospray::cpp;

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

OSPTextureFormat osprayTextureFormat(int depth, int channels,
                                     bool preferLinear = false);

opp::Texture LoadPtexTexture(std::string file);

/// @brief  Load OIIO Texture
/// @param filename
/// @param nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
std::pair<opp::Texture, unsigned char*>
LoadOIIOTexture2D(std::string file, std::string channels = "",
                  bool nearestFilter = false, bool complement = false);

/// @brief
/// @param filename
/// @param number of udim tile columns
/// @param number of udim tile rows
/// @param use nearestFilter or interpolation
/// @param compute 1.f-val.  float only.
/// @return OSPRay texture object, data pointer
std::pair<opp::Texture, char*> LoadUDIMTexture2D(std::string file, int& numX,
                                                 int& numY,
                                                 bool nearestFilter = false,
                                                 bool complement = false);