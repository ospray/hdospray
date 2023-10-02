// Copyright 2018 Pixar
// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_USING_DIRECTIVE

class HdOSPRayRenderBuffer : public HdRenderBuffer {
public:
    HdOSPRayRenderBuffer(SdfPath const& id);
    ~HdOSPRayRenderBuffer() = default;

    virtual bool
    Allocate(GfVec3i const& dimensions, HdFormat format,
             bool multiSampled /*always false with OSPRay*/) override;

    virtual unsigned int GetWidth() const override
    {
        return _width;
    }

    virtual unsigned int GetHeight() const override
    {
        return _height;
    }

    virtual unsigned int GetDepth() const override
    {
        return 1;
    }

    virtual HdFormat GetFormat() const override
    {
        return _format;
    }

    virtual bool IsMultiSampled() const override
    {
        return _multiSampled;
    }

    virtual void* Map() override
    {
        _mappers++;
        return _buffer.data();
    }

    virtual void Unmap() override
    {
        _mappers--;
    }

    virtual bool IsMapped() const override
    {
        return _mappers.load() != 0;
    }

    virtual bool IsConverged() const override
    {
        return _converged.load();
    }

    void SetConverged(bool cv)
    {
        _converged.store(cv);
    }

    virtual void Resolve() override;

    // ---------------------------------------------------------------------- //
    /// \name I/O helpers
    // ---------------------------------------------------------------------- //

    /// Write a float, vec2f, vec3f, or vec4f to the renderbuffer.
    /// This should only be called on a mapped buffer. Extra components will
    /// be silently discarded; if not enough are provided for the buffer, the
    /// remainder will be taken as 0.
    ///   \param pixel         What index to write
    ///   \param numComponents The arity of the value to write.
    ///   \param value         A float-valued vector to write.
    void Write(GfVec3i const& pixel, size_t numComponents, float const* value);

    /// Write an int, vec2i, vec3i, or vec4i to the renderbuffer.
    /// This should only be called on a mapped buffer. Extra components will
    /// be silently discarded; if not enough are provided for the buffer, the
    /// remainder will be taken as 0.
    ///   \param pixel         What index to write
    ///   \param numComponents The arity of the value to write.
    ///   \param value         An int-valued vector to write.
    void Write(GfVec3i const& pixel, size_t numComponents, int const* value);

    /// Clear the renderbuffer with a float, vec2f, vec3f, or vec4f.
    /// This should only be called on a mapped buffer. Extra components will
    /// be silently discarded; if not enough are provided for the buffer, the
    /// remainder will be taken as 0.
    ///   \param numComponents The arity of the value to write.
    ///   \param value         A float-valued vector to write.
    void Clear(size_t numComponents, float const* value);

    /// Clear the renderbuffer with an int, vec2i, vec3i, or vec4i.
    /// This should only be called on a mapped buffer. Extra components will
    /// be silently discarded; if not enough are provided for the buffer, the
    /// remainder will be taken as 0.
    ///   \param numComponents The arity of the value to write.
    ///   \param value         An int-valued vector to write.
    void Clear(size_t numComponents, int const* value);

private:
    // Calculate the needed buffer size, given the allocation parameters.
    static size_t _GetBufferSize(GfVec2i const& dims, HdFormat format);

    // Return the sample format for the given buffer format. Sample buffers
    // are always float32 or int32, but with the same number of components
    // as the base format.
    static HdFormat _GetSampleFormat(HdFormat format);

    // Release any allocated resources.
    virtual void _Deallocate() override;

    // Buffer width.
    unsigned int _width;
    // Buffer height.
    unsigned int _height;
    // Buffer format.
    HdFormat _format;
    // Whether the buffer is operating in multisample mode.
    bool _multiSampled; // currently always false for OSPRay

    // The resolved output buffer.
    std::vector<uint8_t> _buffer;
    // For multisampled buffers: the input write buffer.
    std::vector<uint8_t> _sampleBuffer;
    // For multisampled buffers: the sample count buffer.
    std::vector<uint8_t> _sampleCount;

    // The number of callers mapping this buffer.
    std::atomic<int> _mappers;
    // Whether the buffer has been marked as converged.
    std::atomic<bool> _converged;
};
