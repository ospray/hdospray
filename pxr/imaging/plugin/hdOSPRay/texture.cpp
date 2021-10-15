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

#include "texture.h"
#include "config.h"

#include <rkcommon/math/vec.h>

#include <OpenImageIO/imageio.h>

using namespace rkcommon::math;

OIIO_NAMESPACE_USING

PXR_NAMESPACE_OPEN_SCOPE

OSPTextureFormat
osprayTextureFormat(int depth, int channels, bool preferLinear)
{
    if (depth == 1) {
        if (channels == 1)
            return preferLinear ? OSP_TEXTURE_R8 : OSP_TEXTURE_L8;
        if (channels == 2)
            return preferLinear ? OSP_TEXTURE_RA8 : OSP_TEXTURE_LA8;
        if (channels == 3)
            return preferLinear ? OSP_TEXTURE_RGB8 : OSP_TEXTURE_SRGB;
        if (channels == 4)
            return preferLinear ? OSP_TEXTURE_RGBA8 : OSP_TEXTURE_SRGBA;
    } else if (depth == 4) {
        if (channels == 1)
            return OSP_TEXTURE_R32F;
        if (channels == 3)
            return OSP_TEXTURE_RGB32F;
        if (channels == 4)
            return OSP_TEXTURE_RGBA32F;
    }

    return OSP_TEXTURE_FORMAT_INVALID;
}

/// creates ptex texture and sets to file, does not commit
opp::Texture
LoadPtexTexture(std::string file)
{
    if (file == "")
        return nullptr;
    opp::Texture ospTexture = opp::Texture("ptex");
    ospTexture.setParam("filename", file);
    ospTexture.setParam("sRGB", true);
    return ospTexture;
}

// creates 2d osptexture from file, does not commit
opp::Texture
LoadOIIOTexture2D(std::string file, bool nearestFilter)
{
    std::cout << "loading " << file << std::endl;
    auto in = ImageInput::open(file.c_str());
    if (!in) {
        std::cerr << "#osp: failed to load texture '" + file + "'" << std::endl;
        return nullptr;
    }
    std::cout << "loaded " << file << std::endl;

    const ImageSpec& spec = in->spec();
    vec2i size;
    size.x = spec.width;
    size.y = spec.height;
    int channels = spec.nchannels;
    const bool hdr = spec.format.size() > 1;
    int depth = hdr ? 4 : 1;
    const size_t stride = size.x * channels * depth;
    unsigned char* data
           = (unsigned char*)malloc(sizeof(unsigned char) * size.y * stride);

    in->read_image(hdr ? TypeDesc::FLOAT : TypeDesc::UINT8, data);
    in->close();
#if OIIO_VERSION < 10903
    ImageInput::destroy(in);
#endif

    // flip image (because OSPRay's textures have the origin at the lower left
    // corner)
    for (int y = 0; y < size.y / 2; y++) {
        unsigned char* src = &data[y * stride];
        unsigned char* dest = &data[(size.y - 1 - y) * stride];
        for (size_t x = 0; x < stride; x++)
            std::swap(src[x], dest[x]);
    }
    OSPTextureFormat format = osprayTextureFormat(depth, channels);

    OSPDataType dataType = OSP_UNKNOWN;
    if (format == OSP_TEXTURE_R32F)
        dataType = OSP_FLOAT;
    else if (format == OSP_TEXTURE_RGB32F)
        dataType = OSP_VEC3F;
    else if (format == OSP_TEXTURE_RGBA32F)
        dataType = OSP_VEC4F;
    else if ((format == OSP_TEXTURE_R8) || (format == OSP_TEXTURE_L8))
        dataType = OSP_UCHAR;
    else if ((format == OSP_TEXTURE_RGB8) || (format == OSP_TEXTURE_SRGB))
        dataType = OSP_VEC3UC;
    else if (format == OSP_TEXTURE_RGBA8 || format == OSP_TEXTURE_SRGBA)
        dataType = OSP_VEC4UC;
    else {
        std::cout << "Texture: file: " << file << "\tdepth: " << depth
                  << "\tchannels: " << channels << "\format: " << format
                  << std::endl;
        throw std::runtime_error("hdOSPRay::LoadOIIOTexture2D: \
                                         Unknown texture format");
    }
    opp::SharedData ospData = opp::SharedData(data, dataType, size);
    ospData.commit();

    opp::Texture ospTexture = opp::Texture("texture2d");
    ospTexture.setParam("format", format);
    ospTexture.setParam("filter",
                        nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                      : OSP_TEXTURE_FILTER_BILINEAR);
    ospTexture.setParam("data", ospData);
    ospTexture.commit();

    // TODO: free data!!!

    return ospTexture;
}

PXR_NAMESPACE_CLOSE_SCOPE