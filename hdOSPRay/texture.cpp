// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "texture.h"
#include "config.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>

#include <rkcommon/math/vec.h>

using namespace rkcommon::math;

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

HdOSPRayTexture
LoadHioTexture2D(const std::string file, const std::string channelsStr, bool nearestFilter,
                  bool complement)
{
    const auto image = HioImage::OpenForReading(file);
    if (!image) {
        TF_DEBUG_MSG(OSP, "#osp: failed to load texture \"%s\"\n", file.c_str());
        return HdOSPRayTexture();
    }

    HioImage::StorageSpec desc;
    desc.format = image->GetFormat();
    desc.width = image->GetWidth();
    desc.height = image->GetHeight();
    desc.depth = 1;
    desc.flipped = true;

    vec2i size;
    size.x = desc.width;
    size.y = desc.height;
    const bool srgb = image->IsColorSpaceSRGB();
    int depth = 1;
    if (desc.format == HioFormatFloat16
        || desc.format == HioFormatFloat16Vec2
        || desc.format == HioFormatFloat16Vec3
        || desc.format == HioFormatFloat16Vec4
        || desc.format == HioFormatUInt16
        || desc.format == HioFormatUInt16Vec2
        || desc.format == HioFormatUInt16Vec3
        || desc.format == HioFormatUInt16Vec4
        || desc.format == HioFormatInt16
        || desc.format == HioFormatInt16Vec2
        || desc.format == HioFormatInt16Vec3
        || desc.format == HioFormatInt16Vec4
        )
        depth = 2;
    if (desc.format == HioFormatFloat32
        || desc.format == HioFormatFloat32Vec2
        || desc.format == HioFormatFloat32Vec3
        || desc.format == HioFormatFloat32Vec4
        || desc.format == HioFormatUInt32
        || desc.format == HioFormatUInt32Vec2
        || desc.format == HioFormatUInt32Vec3
        || desc.format == HioFormatUInt32Vec4
        || desc.format == HioFormatInt32
        || desc.format == HioFormatInt32Vec2
        || desc.format == HioFormatInt32Vec3
        || desc.format == HioFormatInt32Vec4
        )
        depth = 4;
    int channels = image->GetBytesPerPixel() / depth;
    const size_t stride = size.x * image->GetBytesPerPixel() * depth;
    auto* data = new uint8_t[sizeof(char) * size.y * stride];
    uint8_t* outData = nullptr;
    desc.data = data;

    bool loaded = image->Read(desc);
    if (!loaded) {
        TF_DEBUG_MSG(OSP, "#osp: failed to load texture \"%s\"\n", file.c_str());
        return HdOSPRayTexture();
    }

    const int outChannels
           = channelsStr.empty() ? channels : channelsStr.length();
    int outDepth = depth;
    if (outChannels == 1)
        outDepth = 4; // convert to float
    int channelOffset = 0;
    if (channelsStr == "g")
        channelOffset = 1;
    if (channelsStr == "b")
        channelOffset = 2;
    if (channels == 4 && channelsStr == "a")
        channelOffset = 3;
    if (outChannels != channels || outDepth != depth) {
        outData = new uint8_t[sizeof(uint8_t) * size.y * size.x
                                         * outChannels * outDepth];
    }

    OSPTextureFormat format = osprayTextureFormat(outDepth, outChannels, !srgb);

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
        throw std::runtime_error(
               "hdOSPRay::LoadHioOTexture2D: \
                                         Unknown texture format");
    }

    if (complement && (format == OSP_TEXTURE_R32F)) {
        float* tex = (float*)data;
        for (size_t i = 0; i < size.x * size.y; i++)
            tex[i] = 1.f - tex[i];
    }
    // convert to outchannels if needed.
    // supported:
    // rgba, rgba to: rgb, r, g, b, or a
    if (outData) {
        const size_t offsetBytes = channelOffset * depth;
        const size_t inBytes = depth * channels;
        const size_t outBytes = outDepth * outChannels;
        unsigned char* in = (unsigned char*)data + offsetBytes;
        unsigned char* out = outData;
        for (size_t i = 0; i < size.x * size.y; i++) {
            if (outDepth == depth)
                std::copy(in, in + outBytes, out);
            else { // assume conversion to float
                float* vals = (float*)out;
                for (int j = 0; j < outChannels; j++)
                    vals[j] = float(in[j]) / 256.f;
            }
            in += inBytes;
            out += outBytes;
        }
    }

    opp::SharedData ospData
           = opp::SharedData(outData ? outData : data, dataType, size);
    ospData.commit();

    opp::Texture ospTexture = opp::Texture("texture2d");
    ospTexture.setParam("format", format);
    ospTexture.setParam("filter",
                        nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                      : OSP_TEXTURE_FILTER_BILINEAR);
    ospTexture.setParam("data", ospData);
    ospTexture.commit();

    auto dataPtr = std::shared_ptr<uint8_t>(data, std::default_delete<uint8_t[]>());
    auto outDataPtr = std::shared_ptr<uint8_t>(outData, std::default_delete<uint8_t[]>());
    return HdOSPRayTexture(ospTexture, outData ? outDataPtr : dataPtr);
}

struct UDIMTileDesc {
    uint8_t* data;
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
        splitPath = std::make_pair(filePath.substr(0, pos),
                                   filePath.substr(pos + 6));
    if (splitPath.first.empty() && splitPath.second.empty()) {
        TF_WARN("#osp: Broken Udim pattern '%s'.", filePath.c_str());
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
HdOSPRayTexture
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
        const std::string file = std::get<1>(tile);
        const auto image = HioImage::OpenForReading(file);
        if (!image) {
            TF_DEBUG_MSG(OSP, "#osp: failed to load texture \"%s\"\n", file.c_str());
            return HdOSPRayTexture();
        }

        HioImage::StorageSpec desc;
        desc.format = image->GetFormat();
        desc.width = image->GetWidth();
        desc.height = image->GetHeight();
        desc.depth = 1;
        desc.flipped = true;
        vec2i size;
        size.x = desc.width;
        size.y = desc.height;
        const bool srgb = image->IsColorSpaceSRGB();
        int depth = 1;
        if (desc.format == HioFormatFloat16
            || desc.format == HioFormatFloat16Vec2
            || desc.format == HioFormatFloat16Vec3
            || desc.format == HioFormatFloat16Vec4
            || desc.format == HioFormatUInt16
            || desc.format == HioFormatUInt16Vec2
            || desc.format == HioFormatUInt16Vec3
            || desc.format == HioFormatUInt16Vec4
            || desc.format == HioFormatInt16
            || desc.format == HioFormatInt16Vec2
            || desc.format == HioFormatInt16Vec3
            || desc.format == HioFormatInt16Vec4
            )
            depth = 2;
        if (desc.format == HioFormatFloat32
            || desc.format == HioFormatFloat32Vec2
            || desc.format == HioFormatFloat32Vec3
            || desc.format == HioFormatFloat32Vec4
            || desc.format == HioFormatUInt32
            || desc.format == HioFormatUInt32Vec2
            || desc.format == HioFormatUInt32Vec3
            || desc.format == HioFormatUInt32Vec4
            || desc.format == HioFormatInt32
            || desc.format == HioFormatInt32Vec2
            || desc.format == HioFormatInt32Vec3
            || desc.format == HioFormatInt32Vec4
            )
        depth = 4;
        int channels = image->GetBytesPerPixel() / depth;
        int texelSizeCurrent = channels * depth;
        if ((texelSize != 0) && (texelSize != texelSizeCurrent)) {
            TF_WARN("#osp: UDIM::texel sizes do not match");
            // TODO: cleanup mem
            return HdOSPRayTexture();
        }
        texelSize = texelSizeCurrent;
        const size_t stride = size.x * channels * depth;
        const size_t dataSize = sizeof(char) * size.y * stride;
        auto* data = new uint8_t[dataSize];
        desc.data = data;
        dataStride = stride;
        bool loaded = image->Read(desc);
        if (!loaded) {
            TF_WARN("#osp: failed to read texture '%s'", file.c_str());
            return HdOSPRayTexture();
        }

        format = osprayTextureFormat(depth, channels, !srgb);

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
            throw std::runtime_error(
                   "hdOSPRay::texture::LoadTexture2D: \
                                         Unknown texture format");
        }
        if ((udimDataType != OSP_UNKNOWN) && udimDataType != dataType) {
            TF_WARN("UDIM has inconsisntent data types");
            return HdOSPRayTexture();
        }
        udimDataType = dataType;

        UDIMTileDesc udesc;
        udesc.size = size;
        udesc.data = data;
        udesc.offset = std::get<0>(tile);
        int tileX = ((udesc.offset % 10) + 1);
        int tileY = (udesc.offset / 10 + 1);
        int totalX = size.x * tileX;
        int totalY = size.y * tileY;
        numTiles.x = std::max(numTiles.x, tileX);
        numTiles.y = std::max(numTiles.y, tileY);
        totalSize.x = std::max(totalSize.x, totalX);
        totalSize.y = std::max(totalSize.y, totalY);
        udimTileDescs.emplace_back(udesc);
    }

    numX = numTiles.x;
    numY = numTiles.y;
    size_t dataSize = totalSize.x * totalSize.y * texelSize;
    auto* data = new uint8_t[sizeof(uint8_t) * dataSize];

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

    auto dataPtr = std::shared_ptr<uint8_t>(data, std::default_delete<uint8_t[]>());

    return HdOSPRayTexture(ospTexture, dataPtr);
}