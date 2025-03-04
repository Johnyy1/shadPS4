// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include "common/assert.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/amdgpu/resource.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan::LiverpoolToVK {

using Liverpool = AmdGpu::Liverpool;

vk::StencilOp StencilOp(Liverpool::StencilFunc op);

vk::CompareOp CompareOp(Liverpool::CompareFunc func);

bool IsPrimitiveCulled(AmdGpu::PrimitiveType type);

vk::PrimitiveTopology PrimitiveType(AmdGpu::PrimitiveType type);

vk::PolygonMode PolygonMode(Liverpool::PolygonMode mode);

vk::CullModeFlags CullMode(Liverpool::CullMode mode);

vk::BlendFactor BlendFactor(Liverpool::BlendControl::BlendFactor factor);

vk::BlendOp BlendOp(Liverpool::BlendControl::BlendFunc func);

vk::SamplerAddressMode ClampMode(AmdGpu::ClampMode mode);

vk::CompareOp DepthCompare(AmdGpu::DepthCompare comp);

vk::Filter Filter(AmdGpu::Filter filter);

vk::SamplerReductionMode FilterMode(AmdGpu::FilterMode mode);

vk::SamplerMipmapMode MipFilter(AmdGpu::MipFilter filter);

vk::BorderColor BorderColor(AmdGpu::BorderColor color);

vk::ComponentSwizzle ComponentSwizzle(AmdGpu::CompSwizzle comp_swizzle);

vk::ComponentMapping ComponentMapping(AmdGpu::CompMapping comp_mapping);

struct SurfaceFormatInfo {
    AmdGpu::DataFormat data_format;
    AmdGpu::NumberFormat number_format;
    vk::Format vk_format;
    vk::FormatFeatureFlags2 flags;
};
std::span<const SurfaceFormatInfo> SurfaceFormats();

vk::Format SurfaceFormat(AmdGpu::DataFormat data_format, AmdGpu::NumberFormat num_format);

vk::Format AdjustColorBufferFormat(vk::Format base_format,
                                   Liverpool::ColorBuffer::SwapMode comp_swap);

struct DepthFormatInfo {
    Liverpool::DepthBuffer::ZFormat z_format;
    Liverpool::DepthBuffer::StencilFormat stencil_format;
    vk::Format vk_format;
    vk::FormatFeatureFlags2 flags;
};
std::span<const DepthFormatInfo> DepthFormats();

vk::Format DepthFormat(Liverpool::DepthBuffer::ZFormat z_format,
                       Liverpool::DepthBuffer::StencilFormat stencil_format);

vk::ClearValue ColorBufferClearValue(const AmdGpu::Liverpool::ColorBuffer& color_buffer);

static constexpr u16 NumVerticesPerQuad = 4;

inline void EmitQuadToTriangleListIndices(u8* out_ptr, u32 num_vertices) {
    u16* out_data = reinterpret_cast<u16*>(out_ptr);
    for (u16 i = 0; i < num_vertices; i += NumVerticesPerQuad) {
        *out_data++ = i;
        *out_data++ = i + 1;
        *out_data++ = i + 2;
        *out_data++ = i;
        *out_data++ = i + 2;
        *out_data++ = i + 3;
    }
}

template <typename T>
void ConvertQuadToTriangleListIndices(u8* out_ptr, const u8* in_ptr, u32 num_vertices) {
    T* out_data = reinterpret_cast<T*>(out_ptr);
    const T* in_data = reinterpret_cast<const T*>(in_ptr);
    for (u16 i = 0; i < num_vertices; i += NumVerticesPerQuad) {
        *out_data++ = in_data[i];
        *out_data++ = in_data[i + 1];
        *out_data++ = in_data[i + 2];
        *out_data++ = in_data[i];
        *out_data++ = in_data[i + 2];
        *out_data++ = in_data[i + 3];
    }
}

vk::SampleCountFlagBits NumSamples(u32 num_samples, vk::SampleCountFlags supported_flags);

static inline vk::Format PromoteFormatToDepth(vk::Format fmt) {
    if (fmt == vk::Format::eR32Sfloat) {
        return vk::Format::eD32Sfloat;
    } else if (fmt == vk::Format::eR16Unorm) {
        return vk::Format::eD16Unorm;
    } else if (fmt == vk::Format::eR8G8B8A8Unorm) {
        return vk::Format::eR32Uint;
    } else if (fmt == vk::Format::eR8G8B8A8Srgb) {
        return vk::Format::eR32Uint;
    }
    UNREACHABLE();
}

} // namespace Vulkan::LiverpoolToVK
