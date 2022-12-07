// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "texture.h"
#include "config.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>

#include <rkcommon/math/vec.h>

#include <OpenImageIO/imageio.h>

using namespace rkcommon::math;

OIIO_NAMESPACE_USING

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
std::pair<opp::Texture, char*>
LoadOIIOTexture2D(std::string file, bool nearestFilter, bool complement)
{
    auto in = ImageInput::open(file.c_str());
    if (!in) {
        std::cerr << "#osp: failed to load texture '" + file + "'" << std::endl;
        return std::pair<opp::Texture, char*>(nullptr, nullptr);
    }

    const ImageSpec& spec = in->spec();
    vec2i size;
    size.x = spec.width;
    size.y = spec.height;
    int channels = spec.nchannels;
    const bool hdr = spec.format.size() > 1;
    int depth = hdr ? 4 : 1;
    const size_t stride = size.x * channels * depth;
    char* data = (char*)malloc(sizeof(char) * size.y * stride);

    in->read_image(hdr ? TypeDesc::FLOAT : TypeDesc::UINT8, data);
    in->close();
#if OIIO_VERSION < 10903
    ImageInput::destroy(in);
#endif
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

    // flip image (because OSPRay's textures have the origin at the lower left
    // corner)
    // compute complement if enabled
    for (int y = 0; y < size.y / 2; y++) {
        char* src = &data[y * stride];
        char* dest = &data[(size.y - 1 - y) * stride];
        for (size_t x = 0; x < stride; x++)
            std::swap(src[x], dest[x]);
    }
    if (complement && (format == OSP_TEXTURE_R32F)) {
        float* tex = (float*)data;
        for (size_t i = 0; i < size.x * size.y; i++)
            tex[i] = 1.f - tex[i];
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

    return std::pair<opp::Texture, char*>(ospTexture, data);
}

struct UDIMTileDesc {
    char* data { nullptr };
    vec2i size { 0, 0 };
    int offset { -1 };
};

/// UDIM helper, splits udim filepath into individual tile files
/// @param filePath Udim filepath of form ...<UDIM>...  
/// @result computes pairs of form <tile id, texture file>
static std::vector<std::tuple<int, TfToken>>
_ParseUDIMTiles(const std::string& filePath)
{
    std::vector<std::tuple<int, TfToken>> result;

    // split filesnames to get prefix and suffix of form ....<UDIM>...
    auto splitPath = std::make_pair(std::string(), std::string());
    const std::string::size_type pos = filePath.find("<UDIM>");
    if (pos != std::string::npos)
        splitPath = std::make_pair(filePath.substr(0, pos), filePath.substr(pos + 6));
    if (splitPath.first.empty() && splitPath.second.empty()) {
        TF_WARN("Broken Udim pattern '%s'.", filePath.c_str());
        return result;
    }

    ArResolver& resolver = ArGetResolver();

    // add file names to result
    for (int i = 1001; i < 1100; i++) {
        const std::string resolvedPath = resolver.Resolve(
               splitPath.first + std::to_string(i) + splitPath.second);
        if (!resolvedPath.empty()) {
            result.emplace_back(i - 1001, resolvedPath);
        }
    }

    return result;
}

// creates 2d osptexture from file, does not commit
std::pair<opp::Texture, char*>
LoadUDIMTexture2D(std::string file, int& numX, int& numY, bool nearestFilter,
                  bool complement)
{
    auto udimTiles = _ParseUDIMTiles(file);
    std::vector<UDIMTileDesc> udimTileDescs;
    vec2i totalSize { 0, 0 };
    vec2i numTiles { 0, 0 };
    numTiles = { 0, 0 };
    OSPDataType udimDataType = OSP_UNKNOWN;
    size_t dataStride = 0;
    size_t texelSize = 0;
    OSPTextureFormat format = OSP_TEXTURE_FORMAT_INVALID;

    // load tile data  TODO: parallelize
    for (auto tile : udimTiles) {
        std::string file = std::get<1>(tile);
        auto in = ImageInput::open(file.c_str());
        if (!in) {
            std::cerr << "#osp: failed to load texture '" + file + "'"
                      << std::endl;
            return std::pair<opp::Texture, char*>(nullptr, nullptr);
        }

        const ImageSpec& spec = in->spec();
        vec2i size;
        size.x = spec.width;
        size.y = spec.height;
        int channels = spec.nchannels;
        const bool hdr = spec.format.size() > 1;
        int depth = hdr ? 4 : 1;
        int texelSizeCurrent = channels * depth;
        if ((texelSize != 0) && (texelSize != texelSizeCurrent)) {
            std::cerr << "UDIM::texel sizes do not match\n";
            // TODO: cleanup mem
            return std::pair<opp::Texture, char*>(nullptr, nullptr);
        }
        texelSize = texelSizeCurrent;
        const size_t stride = size.x * channels * depth;
        const size_t dataSize = sizeof(char) * size.y * stride;
        char* data = (char*)malloc(dataSize);
        dataStride = stride;

        in->read_image(hdr ? TypeDesc::FLOAT : TypeDesc::UINT8, data);
        in->close();
#if OIIO_VERSION < 10903
        ImageInput::destroy(in);
#endif

        // flip image (because OSPRay's textures have the origin at the lower
        // left corner)
        for (int y = 0; y < size.y / 2; y++) {
            char* src = &data[y * stride];
            char* dest = &data[(size.y - 1 - y) * stride];
            for (size_t x = 0; x < stride; x++)
                std::swap(src[x], dest[x]);
        }
        format = osprayTextureFormat(depth, channels);

        OSPDataType dataType = OSP_UNKNOWN;
        if (format == OSP_TEXTURE_R32F) {
            dataType = OSP_FLOAT;
        } else if (format == OSP_TEXTURE_RGB32F) {
            dataType = OSP_VEC3F;
        } else if (format == OSP_TEXTURE_RGBA32F) {
            dataType = OSP_VEC4F;
        } else if ((format == OSP_TEXTURE_R8) || (format == OSP_TEXTURE_L8)) {
            dataType = OSP_UCHAR;
        } else if ((format == OSP_TEXTURE_RGB8)
                   || (format == OSP_TEXTURE_SRGB)) {
            dataType = OSP_VEC3UC;
        } else if (format == OSP_TEXTURE_RGBA8 || format == OSP_TEXTURE_SRGBA) {
            dataType = OSP_VEC4UC;
        } else {
            std::cout << "Texture: file: " << file << "\tdepth: " << depth
                      << "\tchannels: " << channels << "\format: " << format
                      << std::endl;
            throw std::runtime_error("hdOSPRay::texture::LoadTexture2D: \
                                         Unknown texture format");
        }
        if ((udimDataType != OSP_UNKNOWN) && udimDataType != dataType) {
            std::cerr << "UDIM has inconsisntent data types\n";
            delete data;
            return std::pair<opp::Texture, char*>(nullptr, nullptr);
        }
        udimDataType = dataType;

        UDIMTileDesc desc;
        desc.size = size;
        desc.data = data;
        desc.offset = std::get<0>(tile);
        int tileX = ((desc.offset % 10) + 1);
        int tileY = (desc.offset / 10 + 1);
        int totalX = size.x * tileX;
        int totalY = size.y * tileY;
        numTiles.x = std::max(numTiles.x, tileX);
        numTiles.y = std::max(numTiles.y, tileY);
        totalSize.x = std::max(totalSize.x, totalX);
        totalSize.y = std::max(totalSize.y, totalY);
        udimTileDescs.emplace_back(desc);
    }

    numX = numTiles.x;
    numY = numTiles.y;
    size_t dataSize = totalSize.x * totalSize.y * texelSize;
    char* data = (char*)malloc(sizeof(char) * dataSize);

    // copy tile to main texture
    for (auto tile : udimTileDescs) {
        vec2i startTexels;
        startTexels.x = tile.size.x * (tile.offset % 10);
        startTexels.y = tile.size.y * (tile.offset / 10);
        size_t dataIndex = startTexels.y * totalSize.x + startTexels.x;
        size_t tileIndex = 0;
        for (int y = 0; y < tile.size.y; y++) {
            size_t start = tileIndex * texelSize;
            size_t end = (tileIndex + tile.size.x) * texelSize;
            if (complement && (udimDataType == OSP_FLOAT)) {
                float* tex = (float*)(tile.data + start);
                for (int i = 0; i < tile.size.x; i++)
                    tex[i] = 1.f - tex[i];
            }
            std::copy(tile.data + start, tile.data + end,
                      data + dataIndex * texelSize);
            tileIndex += tile.size.x;
            dataIndex += totalSize.x;
        }
    }

    // create ospray texture object from data
    opp::SharedData ospData = opp::SharedData(data, udimDataType, totalSize);
    ospData.commit();

    opp::Texture ospTexture = opp::Texture("texture2d");
    ospTexture.setParam("format", format);
    ospTexture.setParam("filter",
                        nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                      : OSP_TEXTURE_FILTER_BILINEAR);
    ospTexture.setParam("data", ospData);
    ospTexture.commit();

    return std::pair<opp::Texture, char*>(ospTexture, data);
}