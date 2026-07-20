#include "rendererpch.h"

#include "Device.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <tracy/TracyVulkan.hpp>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>

#include "FrameContext.h"
#include "VulkanWindowSurface.h"
#include "Rendering/Buffer/Buffer.h"
#include "Core/ProfilerContext.h"
#include "Core/Window/Window.h"
#include "CoreLib/Containers/Traits.h"
#include "CoreLib/Utils/MemoryUtils.h"
#include "Rendering/FormatTraits.h"
#include "Imgui/ImguiUI.h"
#include "Rendering/DeletionQueue.h"
#include "Rendering/Commands/RenderCommands.h"

#include <AssetLib/Images/ImageAsset.h>
#include <CoreLib/core.h>
#include <CoreLib/Utils/ContainterUtils.h>

struct DeviceState;

namespace
{
static_assert(ImageSubresourceDescription::ALL_MIPMAPS == VK_REMAINING_MIP_LEVELS,
    "Incorrect value for `ALL_MIPMAPS`");
static_assert(ImageSubresourceDescription::ALL_LAYERS == VK_REMAINING_ARRAY_LAYERS,
    "Incorrect value for `ALL_LAYERS`");
static_assert(SamplerCreateInfo::LOD_MAX == VK_LOD_CLAMP_NONE, "Incorrect value for `LOD_MAX`");

constexpr VkFormat vulkanFormatFromFormat(Format format)
{
    static_assert((u32)Format::Undefined == VK_FORMAT_UNDEFINED);
    static_assert((u32)Format::RG4_UNORM_PACK8 == VK_FORMAT_R4G4_UNORM_PACK8);
    static_assert((u32)Format::RGBA4_UNORM_PACK16 == VK_FORMAT_R4G4B4A4_UNORM_PACK16);
    static_assert((u32)Format::BGRA4_UNORM_PACK16 == VK_FORMAT_B4G4R4A4_UNORM_PACK16);
    static_assert((u32)Format::R5G6B5_UNORM_PACK16 == VK_FORMAT_R5G6B5_UNORM_PACK16);
    static_assert((u32)Format::B5G6R5_UNORM_PACK16 == VK_FORMAT_B5G6R5_UNORM_PACK16);
    static_assert((u32)Format::RGB5A1_UNORM_PACK16 == VK_FORMAT_R5G5B5A1_UNORM_PACK16);
    static_assert((u32)Format::BGR5A1_UNORM_PACK16 == VK_FORMAT_B5G5R5A1_UNORM_PACK16);
    static_assert((u32)Format::A1RGB5_UNORM_PACK16 == VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    static_assert((u32)Format::R8_UNORM == VK_FORMAT_R8_UNORM);
    static_assert((u32)Format::R8_SNORM == VK_FORMAT_R8_SNORM);
    static_assert((u32)Format::R8_USCALED == VK_FORMAT_R8_USCALED);
    static_assert((u32)Format::R8_SSCALED == VK_FORMAT_R8_SSCALED);
    static_assert((u32)Format::R8_UINT == VK_FORMAT_R8_UINT);
    static_assert((u32)Format::R8_SINT == VK_FORMAT_R8_SINT);
    static_assert((u32)Format::R8_SRGB == VK_FORMAT_R8_SRGB);
    static_assert((u32)Format::RG8_UNORM == VK_FORMAT_R8G8_UNORM);
    static_assert((u32)Format::RG8_SNORM == VK_FORMAT_R8G8_SNORM);
    static_assert((u32)Format::RG8_USCALED == VK_FORMAT_R8G8_USCALED);
    static_assert((u32)Format::RG8_SSCALED == VK_FORMAT_R8G8_SSCALED);
    static_assert((u32)Format::RG8_UINT == VK_FORMAT_R8G8_UINT);
    static_assert((u32)Format::RG8_SINT == VK_FORMAT_R8G8_SINT);
    static_assert((u32)Format::RG8_SRGB == VK_FORMAT_R8G8_SRGB);
    static_assert((u32)Format::RGB8_UNORM == VK_FORMAT_R8G8B8_UNORM);
    static_assert((u32)Format::RGB8_SNORM == VK_FORMAT_R8G8B8_SNORM);
    static_assert((u32)Format::RGB8_USCALED == VK_FORMAT_R8G8B8_USCALED);
    static_assert((u32)Format::RGB8_SSCALED == VK_FORMAT_R8G8B8_SSCALED);
    static_assert((u32)Format::RGB8_UINT == VK_FORMAT_R8G8B8_UINT);
    static_assert((u32)Format::RGB8_SINT == VK_FORMAT_R8G8B8_SINT);
    static_assert((u32)Format::RGB8_SRGB == VK_FORMAT_R8G8B8_SRGB);
    static_assert((u32)Format::BGR8_UNORM == VK_FORMAT_B8G8R8_UNORM);
    static_assert((u32)Format::BGR8_SNORM == VK_FORMAT_B8G8R8_SNORM);
    static_assert((u32)Format::BGR8_USCALED == VK_FORMAT_B8G8R8_USCALED);
    static_assert((u32)Format::BGR8_SSCALED == VK_FORMAT_B8G8R8_SSCALED);
    static_assert((u32)Format::BGR8_UINT == VK_FORMAT_B8G8R8_UINT);
    static_assert((u32)Format::BGR8_SINT == VK_FORMAT_B8G8R8_SINT);
    static_assert((u32)Format::BGR8_SRGB == VK_FORMAT_B8G8R8_SRGB);
    static_assert((u32)Format::RGBA8_UNORM == VK_FORMAT_R8G8B8A8_UNORM);
    static_assert((u32)Format::RGBA8_SNORM == VK_FORMAT_R8G8B8A8_SNORM);
    static_assert((u32)Format::RGBA8_USCALED == VK_FORMAT_R8G8B8A8_USCALED);
    static_assert((u32)Format::RGBA8_SSCALED == VK_FORMAT_R8G8B8A8_SSCALED);
    static_assert((u32)Format::RGBA8_UINT == VK_FORMAT_R8G8B8A8_UINT);
    static_assert((u32)Format::RGBA8_SINT == VK_FORMAT_R8G8B8A8_SINT);
    static_assert((u32)Format::RGBA8_SRGB == VK_FORMAT_R8G8B8A8_SRGB);
    static_assert((u32)Format::BGRA8_UNORM == VK_FORMAT_B8G8R8A8_UNORM);
    static_assert((u32)Format::BGRA8_SNORM == VK_FORMAT_B8G8R8A8_SNORM);
    static_assert((u32)Format::BGRA8_USCALED == VK_FORMAT_B8G8R8A8_USCALED);
    static_assert((u32)Format::BGRA8_SSCALED == VK_FORMAT_B8G8R8A8_SSCALED);
    static_assert((u32)Format::BGRA8_UINT == VK_FORMAT_B8G8R8A8_UINT);
    static_assert((u32)Format::BGRA8_SINT == VK_FORMAT_B8G8R8A8_SINT);
    static_assert((u32)Format::BGRA8_SRGB == VK_FORMAT_B8G8R8A8_SRGB);
    static_assert((u32)Format::ABGR8_UNORM_PACK32 == VK_FORMAT_A8B8G8R8_UNORM_PACK32);
    static_assert((u32)Format::ABGR8_SNORM_PACK32 == VK_FORMAT_A8B8G8R8_SNORM_PACK32);
    static_assert((u32)Format::ABGR8_USCALED_PACK32 == VK_FORMAT_A8B8G8R8_USCALED_PACK32);
    static_assert((u32)Format::ABGR8_SSCALED_PACK32 == VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
    static_assert((u32)Format::ABGR8_UINT_PACK32 == VK_FORMAT_A8B8G8R8_UINT_PACK32);
    static_assert((u32)Format::ABGR8_SINT_PACK32 == VK_FORMAT_A8B8G8R8_SINT_PACK32);
    static_assert((u32)Format::ABGR8_SRGB_PACK32 == VK_FORMAT_A8B8G8R8_SRGB_PACK32);
    static_assert((u32)Format::A2RGB10_UNORM_PACK32 == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    static_assert((u32)Format::A2RGB10_SNORM_PACK32 == VK_FORMAT_A2R10G10B10_SNORM_PACK32);
    static_assert((u32)Format::A2RGB10_USCALED_PACK32 == VK_FORMAT_A2R10G10B10_USCALED_PACK32);
    static_assert((u32)Format::A2RGB10_SSCALED_PACK32 == VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
    static_assert((u32)Format::A2RGB10_UINT_PACK32 == VK_FORMAT_A2R10G10B10_UINT_PACK32);
    static_assert((u32)Format::A2RGB10_SINT_PACK32 == VK_FORMAT_A2R10G10B10_SINT_PACK32);
    static_assert((u32)Format::A2BGR10_UNORM_PACK32 == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    static_assert((u32)Format::A2BGR10_SNORM_PACK32 == VK_FORMAT_A2B10G10R10_SNORM_PACK32);
    static_assert((u32)Format::A2BGR10_USCALED_PACK32 == VK_FORMAT_A2B10G10R10_USCALED_PACK32);
    static_assert((u32)Format::A2BGR10_SSCALED_PACK32 == VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
    static_assert((u32)Format::A2BGR10_UINT_PACK32 == VK_FORMAT_A2B10G10R10_UINT_PACK32);
    static_assert((u32)Format::A2BGR10_SINT_PACK32 == VK_FORMAT_A2B10G10R10_SINT_PACK32);
    static_assert((u32)Format::R16_UNORM == VK_FORMAT_R16_UNORM);
    static_assert((u32)Format::R16_SNORM == VK_FORMAT_R16_SNORM);
    static_assert((u32)Format::R16_USCALED == VK_FORMAT_R16_USCALED);
    static_assert((u32)Format::R16_SSCALED == VK_FORMAT_R16_SSCALED);
    static_assert((u32)Format::R16_UINT == VK_FORMAT_R16_UINT);
    static_assert((u32)Format::R16_SINT == VK_FORMAT_R16_SINT);
    static_assert((u32)Format::R16_FLOAT == VK_FORMAT_R16_SFLOAT);
    static_assert((u32)Format::RG16_UNORM == VK_FORMAT_R16G16_UNORM);
    static_assert((u32)Format::RG16_SNORM == VK_FORMAT_R16G16_SNORM);
    static_assert((u32)Format::RG16_USCALED == VK_FORMAT_R16G16_USCALED);
    static_assert((u32)Format::RG16_SSCALED == VK_FORMAT_R16G16_SSCALED);
    static_assert((u32)Format::RG16_UINT == VK_FORMAT_R16G16_UINT);
    static_assert((u32)Format::RG16_SINT == VK_FORMAT_R16G16_SINT);
    static_assert((u32)Format::RG16_FLOAT == VK_FORMAT_R16G16_SFLOAT);
    static_assert((u32)Format::RGB16_UNORM == VK_FORMAT_R16G16B16_UNORM);
    static_assert((u32)Format::RGB16_SNORM == VK_FORMAT_R16G16B16_SNORM);
    static_assert((u32)Format::RGB16_USCALED == VK_FORMAT_R16G16B16_USCALED);
    static_assert((u32)Format::RGB16_SSCALED == VK_FORMAT_R16G16B16_SSCALED);
    static_assert((u32)Format::RGB16_UINT == VK_FORMAT_R16G16B16_UINT);
    static_assert((u32)Format::RGB16_SINT == VK_FORMAT_R16G16B16_SINT);
    static_assert((u32)Format::RGB16_FLOAT == VK_FORMAT_R16G16B16_SFLOAT);
    static_assert((u32)Format::RGBA16_UNORM == VK_FORMAT_R16G16B16A16_UNORM);
    static_assert((u32)Format::RGBA16_SNORM == VK_FORMAT_R16G16B16A16_SNORM);
    static_assert((u32)Format::RGBA16_USCALED == VK_FORMAT_R16G16B16A16_USCALED);
    static_assert((u32)Format::RGBA16_SSCALED == VK_FORMAT_R16G16B16A16_SSCALED);
    static_assert((u32)Format::RGBA16_UINT == VK_FORMAT_R16G16B16A16_UINT);
    static_assert((u32)Format::RGBA16_SINT == VK_FORMAT_R16G16B16A16_SINT);
    static_assert((u32)Format::RGBA16_FLOAT == VK_FORMAT_R16G16B16A16_SFLOAT);
    static_assert((u32)Format::R32_UINT == VK_FORMAT_R32_UINT);
    static_assert((u32)Format::R32_SINT == VK_FORMAT_R32_SINT);
    static_assert((u32)Format::R32_FLOAT == VK_FORMAT_R32_SFLOAT);
    static_assert((u32)Format::RG32_UINT == VK_FORMAT_R32G32_UINT);
    static_assert((u32)Format::RG32_SINT == VK_FORMAT_R32G32_SINT);
    static_assert((u32)Format::RG32_FLOAT == VK_FORMAT_R32G32_SFLOAT);
    static_assert((u32)Format::RGB32_UINT == VK_FORMAT_R32G32B32_UINT);
    static_assert((u32)Format::RGB32_SINT == VK_FORMAT_R32G32B32_SINT);
    static_assert((u32)Format::RGB32_FLOAT == VK_FORMAT_R32G32B32_SFLOAT);
    static_assert((u32)Format::RGBA32_UINT == VK_FORMAT_R32G32B32A32_UINT);
    static_assert((u32)Format::RGBA32_SINT == VK_FORMAT_R32G32B32A32_SINT);
    static_assert((u32)Format::RGBA32_FLOAT == VK_FORMAT_R32G32B32A32_SFLOAT);
    static_assert((u32)Format::R64_UINT == VK_FORMAT_R64_UINT);
    static_assert((u32)Format::R64_SINT == VK_FORMAT_R64_SINT);
    static_assert((u32)Format::R64_FLOAT == VK_FORMAT_R64_SFLOAT);
    static_assert((u32)Format::RG64_UINT == VK_FORMAT_R64G64_UINT);
    static_assert((u32)Format::RG64_SINT == VK_FORMAT_R64G64_SINT);
    static_assert((u32)Format::RG64_FLOAT == VK_FORMAT_R64G64_SFLOAT);
    static_assert((u32)Format::RGB64_UINT == VK_FORMAT_R64G64B64_UINT);
    static_assert((u32)Format::RGB64_SINT == VK_FORMAT_R64G64B64_SINT);
    static_assert((u32)Format::RGB64_FLOAT == VK_FORMAT_R64G64B64_SFLOAT);
    static_assert((u32)Format::RGBA64_UINT == VK_FORMAT_R64G64B64A64_UINT);
    static_assert((u32)Format::RGBA64_SINT == VK_FORMAT_R64G64B64A64_SINT);
    static_assert((u32)Format::RGBA64_FLOAT == VK_FORMAT_R64G64B64A64_SFLOAT);
    static_assert((u32)Format::B10G11R11_UFLOAT_PACK32 == VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    static_assert((u32)Format::E5BGR9_UFLOAT_PACK32 == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
    static_assert((u32)Format::D16_UNORM == VK_FORMAT_D16_UNORM);
    static_assert((u32)Format::X8_D24_UNORM_PACK32 == VK_FORMAT_X8_D24_UNORM_PACK32);
    static_assert((u32)Format::D32_FLOAT == VK_FORMAT_D32_SFLOAT);
    static_assert((u32)Format::S8_UINT == VK_FORMAT_S8_UINT);
    static_assert((u32)Format::D16_UNORM_S8_UINT == VK_FORMAT_D16_UNORM_S8_UINT);
    static_assert((u32)Format::D24_UNORM_S8_UINT == VK_FORMAT_D24_UNORM_S8_UINT);
    static_assert((u32)Format::D32_FLOAT_S8_UINT == VK_FORMAT_D32_SFLOAT_S8_UINT);
    static_assert((u32)Format::BC1_RGB_UNORM_BLOCK == VK_FORMAT_BC1_RGB_UNORM_BLOCK);
    static_assert((u32)Format::BC1_RGB_SRGB_BLOCK == VK_FORMAT_BC1_RGB_SRGB_BLOCK);
    static_assert((u32)Format::BC1_RGBA_UNORM_BLOCK == VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    static_assert((u32)Format::BC1_RGBA_SRGB_BLOCK == VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    static_assert((u32)Format::BC2_UNORM_BLOCK == VK_FORMAT_BC2_UNORM_BLOCK);
    static_assert((u32)Format::BC2_SRGB_BLOCK == VK_FORMAT_BC2_SRGB_BLOCK);
    static_assert((u32)Format::BC3_UNORM_BLOCK == VK_FORMAT_BC3_UNORM_BLOCK);
    static_assert((u32)Format::BC3_SRGB_BLOCK == VK_FORMAT_BC3_SRGB_BLOCK);
    static_assert((u32)Format::BC4_UNORM_BLOCK == VK_FORMAT_BC4_UNORM_BLOCK);
    static_assert((u32)Format::BC4_SNORM_BLOCK == VK_FORMAT_BC4_SNORM_BLOCK);
    static_assert((u32)Format::BC5_UNORM_BLOCK == VK_FORMAT_BC5_UNORM_BLOCK);
    static_assert((u32)Format::BC5_SNORM_BLOCK == VK_FORMAT_BC5_SNORM_BLOCK);
    static_assert((u32)Format::BC6H_UFLOAT_BLOCK == VK_FORMAT_BC6H_UFLOAT_BLOCK);
    static_assert((u32)Format::BC6H_FLOAT_BLOCK == VK_FORMAT_BC6H_SFLOAT_BLOCK);
    static_assert((u32)Format::BC7_UNORM_BLOCK == VK_FORMAT_BC7_UNORM_BLOCK);
    static_assert((u32)Format::BC7_SRGB_BLOCK == VK_FORMAT_BC7_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGB8_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGB8_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGB8A1_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGB8A1_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGBA8_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGBA8_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    static_assert((u32)Format::EAC_R11_UNORM_BLOCK == VK_FORMAT_EAC_R11_UNORM_BLOCK);
    static_assert((u32)Format::EAC_R11_SNORM_BLOCK == VK_FORMAT_EAC_R11_SNORM_BLOCK);
    static_assert((u32)Format::EAC_R11G11_UNORM_BLOCK == VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
    static_assert((u32)Format::EAC_R11G11_SNORM_BLOCK == VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
    static_assert((u32)Format::ASTC_4x4_UNORM_BLOCK == VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_4x4_SRGB_BLOCK == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_5x4_UNORM_BLOCK == VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_5x4_SRGB_BLOCK == VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_5x5_UNORM_BLOCK == VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_5x5_SRGB_BLOCK == VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_6x5_UNORM_BLOCK == VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_6x5_SRGB_BLOCK == VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_6x6_UNORM_BLOCK == VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_6x6_SRGB_BLOCK == VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x5_UNORM_BLOCK == VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x5_SRGB_BLOCK == VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x6_UNORM_BLOCK == VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x6_SRGB_BLOCK == VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x8_UNORM_BLOCK == VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x8_SRGB_BLOCK == VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x5_UNORM_BLOCK == VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x5_SRGB_BLOCK == VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x6_UNORM_BLOCK == VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x6_SRGB_BLOCK == VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x8_UNORM_BLOCK == VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x8_SRGB_BLOCK == VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x10_UNORM_BLOCK == VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x10_SRGB_BLOCK == VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_12x10_UNORM_BLOCK == VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_12x10_SRGB_BLOCK == VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_12x12_UNORM_BLOCK == VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_12x12_SRGB_BLOCK == VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    static_assert((u32)Format::GBGR8_422_UNORM == VK_FORMAT_G8B8G8R8_422_UNORM);
    static_assert((u32)Format::B8G8RG8_422_UNORM == VK_FORMAT_B8G8R8G8_422_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_420_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    static_assert((u32)Format::G8_B8R8_2PLANE_420_UNORM == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_422_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
    static_assert((u32)Format::G8_B8R8_2PLANE_422_UNORM == VK_FORMAT_G8_B8R8_2PLANE_422_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_444_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
    static_assert((u32)Format::R10X6_UNORM_PACK16 == VK_FORMAT_R10X6_UNORM_PACK16);
    static_assert((u32)Format::R10X6G10X6_UNORM_2PACK16 == VK_FORMAT_R10X6G10X6_UNORM_2PACK16);
    static_assert((u32)Format::R10X6G10X6B10X6A10X6_UNORM_4PACK16 == VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
    static_assert(
        (u32)Format::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 == VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 == VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)Format::R12X4_UNORM_PACK16 == VK_FORMAT_R12X4_UNORM_PACK16);
    static_assert((u32)Format::R12X4G12X4_UNORM_2PACK16 == VK_FORMAT_R12X4G12X4_UNORM_2PACK16);
    static_assert((u32)Format::R12X4G12X4B12X4A12X4_UNORM_4PACK16 == VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
    static_assert(
        (u32)Format::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 == VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 == VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)Format::G16B16G16R16_422_UNORM == VK_FORMAT_G16B16G16R16_422_UNORM);
    static_assert((u32)Format::B16G16RG16_422_UNORM == VK_FORMAT_B16G16R16G16_422_UNORM);
    static_assert((u32)Format::G16_B16_R16_3PLANE_420_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
    static_assert((u32)Format::G16_B16R16_2PLANE_420_UNORM == VK_FORMAT_G16_B16R16_2PLANE_420_UNORM);
    static_assert((u32)Format::G16_B16_R16_3PLANE_422_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
    static_assert((u32)Format::G16_B16R16_2PLANE_422_UNORM == VK_FORMAT_G16_B16R16_2PLANE_422_UNORM);
    static_assert((u32)Format::G16_B16_R16_3PLANE_444_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
    static_assert((u32)Format::G8_B8R8_2PLANE_444_UNORM == VK_FORMAT_G8_B8R8_2PLANE_444_UNORM);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16);
    static_assert((u32)Format::G16_B16R16_2PLANE_444_UNORM == VK_FORMAT_G16_B16R16_2PLANE_444_UNORM);
    static_assert((u32)Format::A4RGB4_UNORM_PACK16 == VK_FORMAT_A4R4G4B4_UNORM_PACK16);
    static_assert((u32)Format::A4B4G4R4_UNORM_PACK16 == VK_FORMAT_A4B4G4R4_UNORM_PACK16);
    static_assert((u32)Format::ASTC_4x4_FLOAT_BLOCK == VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_5x4_FLOAT_BLOCK == VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_5x5_FLOAT_BLOCK == VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_6x5_FLOAT_BLOCK == VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_6x6_FLOAT_BLOCK == VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x5_FLOAT_BLOCK == VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x6_FLOAT_BLOCK == VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x8_FLOAT_BLOCK == VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x5_FLOAT_BLOCK == VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x6_FLOAT_BLOCK == VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x8_FLOAT_BLOCK == VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x10_FLOAT_BLOCK == VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_12x10_FLOAT_BLOCK == VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK);
    static_assert((u32)Format::ASTC_12x12_FLOAT_BLOCK == VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK);
    static_assert((u32)Format::A1BGR5_UNORM_PACK16 == VK_FORMAT_A1B5G5R5_UNORM_PACK16);
    static_assert((u32)Format::A8_UNORM == VK_FORMAT_A8_UNORM);
    static_assert((u32)Format::PVRTC1_2BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
    static_assert((u32)Format::PVRTC1_4BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
    static_assert((u32)Format::PVRTC2_2BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
    static_assert((u32)Format::PVRTC2_4BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
    static_assert((u32)Format::PVRTC1_2BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
    static_assert((u32)Format::PVRTC1_4BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
    static_assert((u32)Format::PVRTC2_2BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
    static_assert((u32)Format::PVRTC2_4BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
    static_assert((u32)Format::R8_BOOL_ARM == VK_FORMAT_R8_BOOL_ARM);
    static_assert((u32)Format::RG16_SFIXED5_NV == VK_FORMAT_R16G16_SFIXED5_NV);
    static_assert((u32)Format::R10X6_UINT_PACK16_ARM == VK_FORMAT_R10X6_UINT_PACK16_ARM);
    static_assert((u32)Format::R10X6G10X6_UINT_2PACK16_ARM == VK_FORMAT_R10X6G10X6_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM == VK_FORMAT_R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM);
    static_assert((u32)Format::R12X4_UINT_PACK16_ARM == VK_FORMAT_R12X4_UINT_PACK16_ARM);
    static_assert((u32)Format::R12X4G12X4_UINT_2PACK16_ARM == VK_FORMAT_R12X4G12X4_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM == VK_FORMAT_R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM);
    static_assert((u32)Format::R14X2_UINT_PACK16_ARM == VK_FORMAT_R14X2_UINT_PACK16_ARM);
    static_assert((u32)Format::R14X2G14X2_UINT_2PACK16_ARM == VK_FORMAT_R14X2G14X2_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM == VK_FORMAT_R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM);
    static_assert((u32)Format::R14X2_UNORM_PACK16_ARM == VK_FORMAT_R14X2_UNORM_PACK16_ARM);
    static_assert((u32)Format::R14X2G14X2_UNORM_2PACK16_ARM == VK_FORMAT_R14X2G14X2_UNORM_2PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM == VK_FORMAT_R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM);
    static_assert(
        (u32)Format::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM ==
        VK_FORMAT_G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM);
    static_assert(
        (u32)Format::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM ==
        VK_FORMAT_G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM);

    return (VkFormat)(u32)format;
}

constexpr VkBufferUsageFlags vulkanBufferUsageFromUsage(BufferUsage kind)
{
    ASSERT(!enumHasAll(kind, BufferUsage::Vertex | BufferUsage::Index),
        "Buffer usage cannot include both vertex and index")

    VkBufferUsageFlags flags = 0;
    if (enumHasAny(kind, BufferUsage::Vertex))
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (enumHasAny(kind, BufferUsage::Index))
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (enumHasAny(kind, BufferUsage::Uniform))
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (enumHasAny(kind, BufferUsage::Storage))
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (enumHasAny(kind, BufferUsage::Indirect))
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (enumHasAny(kind, BufferUsage::Source))
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (enumHasAny(kind, BufferUsage::Destination))
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (enumHasAny(kind, BufferUsage::Conditional))
        flags |= VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    if (enumHasAny(kind, BufferUsage::DeviceAddress))
        flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    return flags;
}

constexpr VkImageLayout vulkanImageLayoutFromImageLayout(ImageLayout layout)
{
    switch (layout)
    {
    case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
    case ImageLayout::Attachment: return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    case ImageLayout::Readonly: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    case ImageLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ImageLayout::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageLayout::DepthStencilReadonly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case ImageLayout::DepthAttachment: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case ImageLayout::DepthReadonly: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    case ImageLayout::Source: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageLayout::Destination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
        ASSERT(false, "Unsupported image format")
        break;
    }
    std::unreachable();
}

constexpr VkImageUsageFlags vulkanImageUsageFromImageUsage(ImageUsage usage)
{
    ASSERT(!enumHasAll(usage, ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil),
        "Image usage cannot include both color and depth/stencil")

    static const std::vector<std::pair<ImageUsage, VkImageUsageFlags>> MAPPINGS{
        {ImageUsage::Sampled, VK_IMAGE_USAGE_SAMPLED_BIT},
        {ImageUsage::Color, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
        {ImageUsage::Depth, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        {ImageUsage::Stencil, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        {ImageUsage::Storage, VK_IMAGE_USAGE_STORAGE_BIT},
        {ImageUsage::Source, VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
        {ImageUsage::Destination, VK_IMAGE_USAGE_TRANSFER_DST_BIT}
    };

    VkImageUsageFlags flags = 0;
    for (auto&& [iu, vulkanIu] : MAPPINGS)
        if (enumHasAny(usage, iu))
            flags |= vulkanIu;

    return flags;
}

constexpr VkImageAspectFlags vulkanImageAspectFromImageUsage(ImageUsage usage)
{
    if (enumHasAny(usage, ImageUsage::Depth))
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    if (enumHasAny(usage, ImageUsage::Stencil))
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    // todo: this is probably incorrect
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

constexpr VkImageType vulkanImageTypeFromImageKind(ImageKind kind)
{
    switch (kind)
    {
    case ImageKind::Image2d:
    case ImageKind::ImageCubemap:
    case ImageKind::Image2dArray:
        return VK_IMAGE_TYPE_2D;
    case ImageKind::Image3d:
        return VK_IMAGE_TYPE_3D;
    default:
        ASSERT(false, "Unsupported image kind")
        break;
    }
    std::unreachable();
}

constexpr VkImageCreateFlags vulkanImageFlagsFromImageKind(ImageKind kind)
{
    VkImageCreateFlags flags = 0;
    if (kind == ImageKind::ImageCubemap)
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    return flags;
}

constexpr VkImageViewType vulkanImageViewTypeFromImageKind(ImageKind kind)
{
    switch (kind)
    {
    case ImageKind::Image2d:
        return VK_IMAGE_VIEW_TYPE_2D;
    case ImageKind::Image3d:
        return VK_IMAGE_VIEW_TYPE_3D;
    case ImageKind::ImageCubemap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case ImageKind::Image2dArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    default:
        ASSERT(false, "Unsupported image kind")
        break;
    }
    std::unreachable();
}

constexpr VkImageViewType vulkanImageViewTypeFromImageAndViewKind(ImageKind kind, ImageViewKind viewKind)
{
    switch (viewKind)
    {
    case ImageViewKind::Inherit:
        return vulkanImageViewTypeFromImageKind(kind);
    case ImageViewKind::Image2d:
    case ImageViewKind::Image3d:
    case ImageViewKind::Cubemap:
    case ImageViewKind::Image2dArray:
        return vulkanImageViewTypeFromImageKind((ImageKind)viewKind);
    default:
        ASSERT(false, "Unsupported image view kind")
        break;
    }
    std::unreachable();
}

constexpr VkFilter vulkanFilterFromImageFilter(ImageFilter filter)
{
    switch (filter)
    {
    case ImageFilter::Linear:
        return VK_FILTER_LINEAR;
    case ImageFilter::Nearest:
        return VK_FILTER_NEAREST;
    default:
        ASSERT(false, "Unsupported filter format")
    }
    std::unreachable();
}

constexpr VkSamplerMipmapMode vulkanMipmapModeFromSamplerFilter(VkFilter filter)
{
    switch (filter)
    {
    case VK_FILTER_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case VK_FILTER_LINEAR:
    case VK_FILTER_CUBIC_IMG:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        ASSERT(false, "Unsupported filter format")
    }
    std::unreachable();
}

constexpr VkSamplerReductionMode vulkanSamplerReductionModeFromSamplerReductionMode(SamplerReductionMode mode)
{
    switch (mode)
    {
    case SamplerReductionMode::Average:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    case SamplerReductionMode::Min:
        return VK_SAMPLER_REDUCTION_MODE_MIN;
    case SamplerReductionMode::Max:
        return VK_SAMPLER_REDUCTION_MODE_MAX;
    default:
        ASSERT(false, "Unsupported sampler reduction mode")
    }
    std::unreachable();
}

constexpr VkCompareOp vulkanSamplerCompareOpFromSamplerDepthCompareMode(SamplerDepthCompareMode mode)
{
    switch (mode)
    {
    case SamplerDepthCompareMode::None:
        return VK_COMPARE_OP_NEVER;
    case SamplerDepthCompareMode::Less:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case SamplerDepthCompareMode::Greater:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    default:
        ASSERT(false, "Unsupported sampler depth compare mode")
    }
    std::unreachable();
}

constexpr bool isVulkanSamplerCompareOpEnabledFromSamplerDepthCompareMode(SamplerDepthCompareMode mode)
{
    return mode != SamplerDepthCompareMode::None;
}

constexpr VkSamplerAddressMode vulkanSamplerAddressModeFromSamplerWrapMode(SamplerWrapMode mode)
{
    switch (mode)
    {
    case SamplerWrapMode::ClampEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SamplerWrapMode::ClampBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case SamplerWrapMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:
        ASSERT(false, "Unsupported sampler wrap mode")
    }
    std::unreachable();
}

constexpr VkBorderColor vulkanBorderColorFromBorderColor(SamplerBorderColor color)
{
    switch (color)
    {
    case SamplerBorderColor::White:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case SamplerBorderColor::Black:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    default:
        ASSERT(false, "Unsupported attachment load")
    }
    std::unreachable();
}

constexpr VkAttachmentLoadOp vulkanAttachmentLoadFromAttachmentLoad(AttachmentLoad load)
{
    switch (load)
    {
    case AttachmentLoad::Unspecified:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case AttachmentLoad::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case AttachmentLoad::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    default:
        ASSERT(false, "Unsupported attachment load")
    }
    std::unreachable();
}

constexpr VkAttachmentStoreOp vulkanAttachmentStoreFromAttachmentStore(AttachmentStore store)
{
    switch (store)
    {
    case AttachmentStore::Unspecified:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case AttachmentStore::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        ASSERT(false, "Unsupported attachment load")
    }
    std::unreachable();
}

constexpr VkCommandBufferLevel vulkanBufferLevelFromBufferKind(CommandBufferKind kind)
{
    switch (kind)
    {
    case CommandBufferKind::Primary: return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    case CommandBufferKind::Secondary: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    default:
        ASSERT(false, "Unrecognized command buffer kind")
        break;
    }
    std::unreachable();
}

VkCommandBufferUsageFlags vulkanCommandBufferFlagsFromUsage(CommandBufferUsage usage)
{
    VkCommandBufferUsageFlags flags = 0;
    if ((usage & CommandBufferUsage::SingleSubmit) == CommandBufferUsage::SingleSubmit)
        flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if ((usage & CommandBufferUsage::SimultaneousUse) == CommandBufferUsage::SimultaneousUse)
        flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    return flags;
}

constexpr VkPipelineStageFlags2 vulkanPipelineStageFromPipelineStage(PipelineStage stage)
{
    static const std::vector<std::pair<PipelineStage, VkPipelineStageFlags2>> MAPPINGS{
        {PipelineStage::Top, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT},
        {PipelineStage::Indirect, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT},
        {PipelineStage::VertexInput, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT},
        {PipelineStage::IndexInput, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT},
        {PipelineStage::AttributeInput, VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT},
        {PipelineStage::VertexShader, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT},
        {PipelineStage::HullShader, VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT},
        {PipelineStage::DomainShader, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT},
        {PipelineStage::GeometryShader, VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT},
        {PipelineStage::PixelShader, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
        {PipelineStage::DepthEarly, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
        {PipelineStage::DepthLate, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT},
        {PipelineStage::ColorOutput, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
        {PipelineStage::ComputeShader, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        {PipelineStage::Copy, VK_PIPELINE_STAGE_2_COPY_BIT},
        {PipelineStage::Blit, VK_PIPELINE_STAGE_2_BLIT_BIT},
        {PipelineStage::Resolve, VK_PIPELINE_STAGE_2_RESOLVE_BIT},
        {PipelineStage::Clear, VK_PIPELINE_STAGE_2_CLEAR_BIT},
        {PipelineStage::AllTransfer, VK_PIPELINE_STAGE_2_TRANSFER_BIT},
        {PipelineStage::AllGraphics, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
        {PipelineStage::AllPreRasterization, VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT},
        {PipelineStage::AllCommands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
        {PipelineStage::Bottom, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT},
        {PipelineStage::Host, VK_PIPELINE_STAGE_2_HOST_BIT},
        {PipelineStage::TransformFeedback, VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT},
        {PipelineStage::ConditionalRendering, VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT},
    };

    VkPipelineStageFlags2 flags = 0;
    for (auto&& [ps, vulkanPs] : MAPPINGS)
        if (enumHasAny(stage, ps))
            flags |= vulkanPs;

    return flags;
}

constexpr VkAccessFlagBits2 vulkanAccessFlagsFromPipelineAccess(PipelineAccess access)
{
    static const std::vector<std::pair<PipelineAccess, VkAccessFlagBits2>> MAPPINGS{
        {PipelineAccess::ReadIndirect, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
        {PipelineAccess::ReadIndex, VK_ACCESS_2_INDEX_READ_BIT},
        {PipelineAccess::ReadAttribute, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT},
        {PipelineAccess::ReadUniform, VK_ACCESS_2_UNIFORM_READ_BIT},
        {PipelineAccess::ReadInputAttachment, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT},
        {PipelineAccess::ReadColorAttachment, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT},
        {PipelineAccess::ReadDepthStencilAttachment, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT},
        {PipelineAccess::ReadTransfer, VK_ACCESS_2_TRANSFER_READ_BIT},
        {PipelineAccess::ReadHost, VK_ACCESS_2_HOST_READ_BIT},
        {PipelineAccess::ReadSampled, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
        {PipelineAccess::ReadStorage, VK_ACCESS_2_SHADER_STORAGE_READ_BIT},
        {PipelineAccess::ReadShader, VK_ACCESS_2_SHADER_READ_BIT},
        {PipelineAccess::ReadAll, VK_ACCESS_2_MEMORY_READ_BIT},
        {PipelineAccess::WriteColorAttachment, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
        {PipelineAccess::WriteDepthStencilAttachment, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
        {PipelineAccess::WriteTransfer, VK_ACCESS_2_TRANSFER_WRITE_BIT},
        {PipelineAccess::WriteHost, VK_ACCESS_2_HOST_WRITE_BIT},
        {PipelineAccess::WriteShader, VK_ACCESS_2_SHADER_WRITE_BIT},
        {PipelineAccess::WriteAll, VK_ACCESS_2_MEMORY_WRITE_BIT},
        {PipelineAccess::ReadFeedbackCounter, VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT},
        {PipelineAccess::WriteFeedbackCounter, VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT},
        {PipelineAccess::WriteFeedback, VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT},
        {PipelineAccess::ReadConditional, VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT},
    };

    VkAccessFlagBits2 flags = 0;
    for (auto&& [a, vulkanA] : MAPPINGS)
        if (enumHasAny(access, a))
            flags |= vulkanA;

    return flags;
}

constexpr VkDependencyFlags vulkanDependencyFlagsFromPipelineDependencyFlags(
    PipelineDependencyFlags dependencyFlags)
{
    static const std::vector<std::pair<PipelineDependencyFlags, VkDependencyFlags>> MAPPINGS{
        {PipelineDependencyFlags::ByRegion, VK_DEPENDENCY_BY_REGION_BIT},
        {PipelineDependencyFlags::DeviceGroup, VK_DEPENDENCY_DEVICE_GROUP_BIT},
        {PipelineDependencyFlags::FeedbackLoop, VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT},
        {PipelineDependencyFlags::LocalView, VK_DEPENDENCY_VIEW_LOCAL_BIT},
    };

    VkDependencyFlags flags = 0;
    for (auto&& [d, vulkanD] : MAPPINGS)
        if (enumHasAny(dependencyFlags, d))
            flags |= vulkanD;

    return flags;
}

constexpr VkDescriptorType vulkanDescriptorTypeFromDescriptorType(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorType::Image: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorType::ImageStorage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::TexelUniform: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case DescriptorType::TexelStorage: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::UniformBufferDynamic: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorType::StorageBufferDynamic: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorType::Input: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    default:
        ASSERT(false, "Unsupported descriptor type")
        break;
    }
    std::unreachable();
}

constexpr VkDescriptorBindingFlags vulkanDescriptorBindingFlagsFromDescriptorFlags(DescriptorFlags flags)
{
    VkDescriptorBindingFlags bindingFlags = 0;
    if (enumHasAny(flags, DescriptorFlags::UpdateAfterBind))
        bindingFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    if (enumHasAny(flags, DescriptorFlags::UpdateUnusedPending))
        bindingFlags |= VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    if (enumHasAny(flags, DescriptorFlags::PartiallyBound))
        bindingFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    if (enumHasAny(flags, DescriptorFlags::VariableCount))
        bindingFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;

    return bindingFlags;
}

constexpr VkDescriptorSetLayoutCreateFlags vulkanDescriptorsLayoutFlagsFromDescriptorsLayoutFlags(
    DescriptorLayoutFlags flags)
{
    VkDescriptorSetLayoutCreateFlags setFlags = 0;
    if (enumHasAny(flags, DescriptorLayoutFlags::UpdateAfterBind))
        setFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    if (enumHasAny(flags, DescriptorLayoutFlags::DescriptorBuffer))
        setFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    if (enumHasAny(flags, DescriptorLayoutFlags::EmbeddedImmutableSamplers))
        setFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;

    return setFlags;
}

#ifndef DESCRIPTOR_BUFFER
constexpr VkDescriptorPoolCreateFlags vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(DescriptorPoolFlags flags)
{
    VkDescriptorPoolCreateFlags poolFlags = 0;
    if (enumHasAny(flags, DescriptorPoolFlags::UpdateAfterBind))
        poolFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    if (enumHasAny(flags, DescriptorPoolFlags::HostOnly))
        poolFlags |= VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT;

    return poolFlags;
}
#endif

constexpr VkShaderStageFlags vulkanShaderStageFromShaderStage(ShaderStage stage)
{
    VkShaderStageFlags flags = 0;
    if (enumHasAny(stage, ShaderStage::Vertex))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (enumHasAny(stage, ShaderStage::Pixel))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (enumHasAny(stage, ShaderStage::Compute))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;

    return flags;
}

constexpr VkShaderStageFlagBits vulkanStageBitFromShaderStage(ShaderStage stage)
{
    ASSERT(((u32)stage & (u32(stage) - 1)) == 0, "At this point, stage should represent a single (unmerged) shader")
    switch (stage)
    {
    case ShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::Pixel:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStage::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        ASSERT(false, "Unrecognized shader kind")
    }
    std::unreachable();
}

constexpr std::vector<VkDynamicState> vulkanDynamicStatesFromDynamicStates(DynamicStates states)
{
    std::vector<VkDynamicState> vulkanStates;

    if (enumHasAny(states, DynamicStates::Viewport))
        vulkanStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    if (enumHasAny(states, DynamicStates::Scissor))
        vulkanStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    if (enumHasAny(states, DynamicStates::DepthBias))
        vulkanStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

    return vulkanStates;
}

constexpr VkCullModeFlags vulkanCullModeFromFaceCullMode(FaceCullMode mode)
{
    switch (mode)
    {
    case FaceCullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case FaceCullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case FaceCullMode::None:
        return VK_CULL_MODE_NONE;
    default:
        ASSERT(false, "Unrecognized face cull mode")
    }
    std::unreachable();
}

constexpr VkPrimitiveTopology vulkanTopologyFromPrimitiveKind(PrimitiveKind kind)
{
    switch (kind)
    {
    case PrimitiveKind::Triangle:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveKind::Point:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    default:
        ASSERT(false, "Unrecognized primitive kind")
        break;
    }
    std::unreachable();
}

struct SurfaceDetails
{
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;

    bool IsSufficient()
    {
        return !(Formats.empty() || PresentModes.empty());
    }
};

SurfaceDetails getSurfaceDetails(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    SurfaceDetails details = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &details.Capabilities);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.Formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, details.Formats.data());
    }

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.PresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, details.PresentModes.data());
    }

    return details;
}

std::string_view vkResultToString(VkResult result)
{
    switch (result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_NOT_PERMITTED:
        return "VK_ERROR_NOT_PERMITTED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
        return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
        return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
        return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
        return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
        return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
        return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
    case VK_PIPELINE_BINARY_MISSING_KHR:
        return "VK_PIPELINE_BINARY_MISSING_KHR";
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
        return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
    default:
        return "Unknown";
    }
}

void deviceCheck(VkResult res, std::string_view message)
{
    if (res != VK_SUCCESS)
        LUX_LOG_FATAL("Device check failed with result {}: {}", vkResultToString(res), message.data());
}
}

struct SwapchainResource
{
    using ObjectType = SwapchainTag;
    VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
    VkFormat ColorFormat{};
    SwapchainDescription Description{};
    std::vector<Semaphore> RenderSemaphores{};
};

struct BufferResource
{
    using ObjectType = BufferTag;
    VkBuffer Buffer{VK_NULL_HANDLE};
    BufferDescription Description{};
    void* HostAddress{nullptr};
    VmaAllocation Allocation{VK_NULL_HANDLE};
};

struct BufferArenaResource
{
    using ObjectType = BufferArenaTag;
    VmaVirtualBlock VirtualBlock{VK_NULL_HANDLE};
    Buffer Buffer{};
    u64 VirtualSizeBytes{};
};

struct ImageResource
{
    using ObjectType = ImageTag;

    struct ViewsInfo
    {
        union ViewType
        {
            u64 ViewCount;
            VkImageView View{VK_NULL_HANDLE};
        };

        ViewType ViewType;
        // in case of multiple views ViewList points to array of multiple views,
        // in case of single view it points to ViewType, as a result you can always dereference
        // ViewList and get a valid view
        VkImageView* ViewList{nullptr};
    };

    VkImage Image{VK_NULL_HANDLE};
    ViewsInfo Views{};
    ImageDescription Description{};
    VmaAllocation Allocation{VK_NULL_HANDLE};

    friend void swap(ImageResource& a, ImageResource& b) noexcept
    {
        std::swap(a.Image, b.Image);
        std::swap(a.Description, b.Description);
        std::swap(a.Allocation, b.Allocation);

        const bool aSelfPointer = a.Views.ViewList == &a.Views.ViewType.View;
        const bool bSelfPointer = b.Views.ViewList == &b.Views.ViewType.View;

        std::swap(a.Views.ViewType, b.Views.ViewType);
        std::swap(a.Views.ViewList, b.Views.ViewList);
        /* swap is intentional */
        if (aSelfPointer)
            b.Views.ViewList = &b.Views.ViewType.View;
        if (bSelfPointer)
            a.Views.ViewList = &a.Views.ViewType.View;
    }
};

struct SamplerResource
{
    using ObjectType = SamplerTag;
    VkSampler Sampler{VK_NULL_HANDLE};
};

struct CommandPoolResource
{
    using ObjectType = CommandPoolTag;
    VkCommandPool CommandPool{VK_NULL_HANDLE};
};

struct CommandBufferResource
{
    using ObjectType = CommandBufferTag;
    VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
    CommandBufferKind Kind{CommandBufferKind::Primary};
};

struct DescriptorsLayoutResource
{
    using ObjectType = DescriptorsLayoutTag;
    VkDescriptorSetLayout Layout{VK_NULL_HANDLE};
};
#ifdef DESCRIPTOR_BUFFER
struct DescriptorsResource
{
    using ObjectType = DescriptorsTag;
    std::vector<u64> Offsets{};
    u64 SizeBytes{0};
    DescriptorArenaAllocator Allocator{};
};
struct DescriptorArenaAllocatorResource
{
    using ObjectType = DescriptorArenaAllocatorTag;
    void* MappedAddress;
    u64 DeviceAddress;
    u64 SizeBytes{0};
    u64 CurrentOffset{0};
    u32 DescriptorSet{0};
    VkBufferUsageFlags DescriptorBufferUsage{};
    DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
    Buffer Arena;
    std::vector<Descriptors> Descriptors;
};
#else
struct DescriptorsResource
{
    using ObjectType = DescriptorsTag;
    VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
    VkDescriptorPool Pool{VK_NULL_HANDLE};
    DescriptorArenaAllocator Allocator{};
    DescriptorsLayout Layout{};
};

struct DescriptorArenaAllocatorResource
{
    using ObjectType = DescriptorArenaAllocatorTag;

    struct PoolInfo
    {
        VkDescriptorPool Pool;
        DescriptorPoolFlags Flags;
    };

    struct PoolSize
    {
        DescriptorType DescriptorType;
        f32 SetSizeMultiplier;
    };

    std::vector<PoolInfo> FreePools;
    std::vector<PoolInfo> UsedPools;
    std::vector<PoolSize> PoolSizes = {
        {DescriptorType::Sampler, 0.5f},
        {DescriptorType::Image, 4.0f},
        {DescriptorType::ImageStorage, 1.0f},
        {DescriptorType::TexelUniform, 1.0f},
        {DescriptorType::TexelStorage, 1.0f},
        {DescriptorType::UniformBuffer, 2.0f},
        {DescriptorType::StorageBuffer, 2.0f},
        {DescriptorType::UniformBufferDynamic, 1.0f},
        {DescriptorType::StorageBufferDynamic, 1.0f},
        {DescriptorType::Input, 0.5f}
    };
    u32 MaxSetsPerPool{};
    std::vector<Descriptors> Descriptors;
};
#endif
struct PipelineLayoutResource
{
    using ObjectType = PipelineLayoutTag;
    VkPipelineLayout Layout{VK_NULL_HANDLE};
    std::vector<VkPushConstantRange> PushConstants;
};

struct PipelineResource
{
    using ObjectType = PipelineTag;
    VkPipeline Pipeline{VK_NULL_HANDLE};
};

struct ShaderModuleResource
{
    using ObjectType = ShaderModuleTag;
    VkShaderModule Module{VK_NULL_HANDLE};
};

struct RenderingAttachmentResource
{
    using ObjectType = RenderingAttachmentTag;
    VkRenderingAttachmentInfo AttachmentInfo{};
};

struct RenderingInfoResource
{
    using ObjectType = RenderingInfoTag;
    std::vector<VkRenderingAttachmentInfo> ColorAttachments{};
    std::optional<VkRenderingAttachmentInfo> DepthAttachment{};
    glm::uvec2 RenderArea{};
};

struct FenceResource
{
    using ObjectType = FenceTag;
    VkFence Fence{VK_NULL_HANDLE};
};

struct SemaphoreResource
{
    using ObjectType = SemaphoreTag;
    VkSemaphore Semaphore{VK_NULL_HANDLE};
};

struct TimelineSemaphoreResource
{
    using ObjectType = TimelineSemaphoreTag;
    VkSemaphore Semaphore{VK_NULL_HANDLE};
    u64 Timeline{0};
};

struct DependencyInfoResource
{
    using ObjectType = DependencyInfoTag;
    static constexpr u32 MAX_MEMORY_BARRIERS = 2;
    VkDependencyInfo DependencyInfo;
    u32 MemoryBarriersCount{0};
    std::array<VkMemoryBarrier2, MAX_MEMORY_BARRIERS> MemoryBarriers{};
    std::optional<VkImageMemoryBarrier2> LayoutDependency;
};

struct SplitBarrierResource
{
    using ObjectType = SplitBarrierTag;
    VkEvent Event{VK_NULL_HANDLE};
};

template <typename Tag>
struct TagTraits
{
    static_assert(!sizeof(Tag), "No match for type");
};

template <>
struct TagTraits<SwapchainTag>
{
    using ResourceType = SwapchainResource;
};

template <>
struct TagTraits<BufferTag>
{
    using ResourceType = BufferResource;
};

template <>
struct TagTraits<BufferArenaTag>
{
    using ResourceType = BufferArenaResource;
};

template <>
struct TagTraits<ImageTag>
{
    using ResourceType = ImageResource;
};

template <>
struct TagTraits<SamplerTag>
{
    using ResourceType = SamplerResource;
};

template <>
struct TagTraits<CommandPoolTag>
{
    using ResourceType = CommandPoolResource;
};

template <>
struct TagTraits<CommandBufferTag>
{
    using ResourceType = CommandBufferResource;
};

template <>
struct TagTraits<DescriptorsLayoutTag>
{
    using ResourceType = DescriptorsLayoutResource;
};

template <>
struct TagTraits<DescriptorsTag>
{
    using ResourceType = DescriptorsResource;
};

template <>
struct TagTraits<DescriptorArenaAllocatorTag>
{
    using ResourceType = DescriptorArenaAllocatorResource;
};

template <>
struct TagTraits<PipelineLayoutTag>
{
    using ResourceType = PipelineLayoutResource;
};

template <>
struct TagTraits<PipelineTag>
{
    using ResourceType = PipelineResource;
};

template <>
struct TagTraits<ShaderModuleTag>
{
    using ResourceType = ShaderModuleResource;
};

template <>
struct TagTraits<RenderingAttachmentTag>
{
    using ResourceType = RenderingAttachmentResource;
};

template <>
struct TagTraits<RenderingInfoTag>
{
    using ResourceType = RenderingInfoResource;
};

template <>
struct TagTraits<FenceTag>
{
    using ResourceType = FenceResource;
};

template <>
struct TagTraits<SemaphoreTag>
{
    using ResourceType = SemaphoreResource;
};

template <>
struct TagTraits<TimelineSemaphoreTag>
{
    using ResourceType = TimelineSemaphoreResource;
};

template <>
struct TagTraits<DependencyInfoTag>
{
    using ResourceType = DependencyInfoResource;
};

template <>
struct TagTraits<SplitBarrierTag>
{
    using ResourceType = SplitBarrierResource;
};

DeviceResources& deviceResources();

class DeviceResources
{
    FRIEND_INTERNAL
    friend class DeviceInternal;
    friend struct DeviceState;

    template <typename T>
    using ResourceContainerType = DeviceSparseSet<T>;

    template <typename T>
    struct ResourceContainerWithLock
    {
        ResourceContainerType<T> Container{};
        std::mutex Mutex{};
    };

private:
    void MapCmdToPool(CommandBuffer cmd, CommandPool pool);
    void DestroyCmdsOfPool(CommandPool pool);

private:
    template <typename... Tags>
    class View
    {
        friend class DeviceResources;

    public:
        template <typename Resource>
        constexpr auto Add(Resource&& resource) const
        {
            using Decayed = std::decay_t<Resource>;
            deviceResources().m_AllocatedCount += 1;
            return GetContainer<typename Decayed::ObjectType>().Insert(std::forward<Resource>(resource));
        }

        template <typename Tag>
        constexpr void Remove(ResourceHandleType<Tag> handle) const
        {
            deviceResources().m_DeallocatedCount += 1;
            GetContainer<Tag>().Erase(handle);
        }

        template <typename Tag>
        constexpr auto& operator[](ResourceHandleType<Tag> handle) const
        {
            return GetContainer<Tag>()[handle];
        }

        template <typename... R>
        operator View<R...>() const
        {
            static_assert(is_unique_v<R...>, "Types must be unique");
            return View<R...>(this->template GetContainer<R>()...);
        }

    protected:
        View(ResourceContainerType<typename TagTraits<Tags>::ResourceType>&... containers)
            : m_Containers(&containers...)
        {
        }

        template <typename Tag>
        auto& GetContainer() const
        {
            using ResourceType = TagTraits<Tag>::ResourceType;
            return *std::get<ResourceContainerType<ResourceType>*>(m_Containers);
        }

    private:
        std::tuple<ResourceContainerType<typename TagTraits<Tags>::ResourceType>*...> m_Containers;
    };

#ifndef NDEBUG
    struct ViewLockHolderGuard
    {
        ViewLockHolderGuard()
        {
            ASSERT(!HoldsLockedView, "Already have active locked view")
            HoldsLockedView = true;
        }

        ~ViewLockHolderGuard() { HoldsLockedView = false; }
        inline static thread_local bool HoldsLockedView{false};
    };
#else
    struct ViewLockHolderGuard
    {
    };
#endif // NDEBUG

    template <typename... Tags>
    class LockedView : public View<Tags...>
    {
        friend class DeviceResources;

        template <typename... R>
        friend class View;

        template <typename T>
        using MutexT = std::mutex;
        using LockType = decltype(std::scoped_lock(std::declval<MutexT<Tags>&>()...));

    public:
        LockedView(ResourceContainerWithLock<typename TagTraits<Tags>::ResourceType>&... containers)
            : View<Tags...>(containers.Container...),
              m_Lock(containers.Mutex...)
        {
        }

        template <typename... R>
        operator View<R...>() const
        {
            static_assert(is_unique_v<R...>, "Types must be unique");
            return View<R...>(this->template GetContainer<R>()...);
        }

    private:
        ViewLockHolderGuard m_LockHolderGuard;
        LockType m_Lock;
    };

    template <typename... Tags>
    auto GetLockedView()
    {
        static_assert(is_unique_v<Tags...>, "Types must be unique");

        return LockedView<Tags...>(GetContainer<Tags>()...);
    }

    template <typename Tag>
    ResourceContainerWithLock<typename TagTraits<Tag>::ResourceType>& GetContainer()
    {
        if constexpr (std::is_same_v<Tag, SwapchainTag>)
            return m_Swapchains;
        else if constexpr (std::is_same_v<Tag, CommandPoolTag>)
            return m_CommandPools;
        else if constexpr (std::is_same_v<Tag, CommandBufferTag>)
            return m_CommandBuffers;
        else if constexpr (std::is_same_v<Tag, BufferTag>)
            return m_Buffers;
        else if constexpr (std::is_same_v<Tag, BufferArenaTag>)
            return m_BufferArenas;
        else if constexpr (std::is_same_v<Tag, ImageTag>)
            return m_Images;
        else if constexpr (std::is_same_v<Tag, SamplerTag>)
            return m_Samplers;
        else if constexpr (std::is_same_v<Tag, RenderingAttachmentTag>)
            return m_RenderingAttachments;
        else if constexpr (std::is_same_v<Tag, RenderingInfoTag>)
            return m_RenderingInfos;
        else if constexpr (std::is_same_v<Tag, PipelineLayoutTag>)
            return m_PipelineLayouts;
        else if constexpr (std::is_same_v<Tag, PipelineTag>)
            return m_Pipelines;
        else if constexpr (std::is_same_v<Tag, ShaderModuleTag>)
            return m_ShaderModules;
        else if constexpr (std::is_same_v<Tag, DescriptorsLayoutTag>)
            return m_DescriptorLayouts;
        else if constexpr (std::is_same_v<Tag, DescriptorArenaAllocatorTag>)
            return m_DescriptorArenaAllocators;
        else if constexpr (std::is_same_v<Tag, DescriptorsTag>)
            return m_Descriptors;
        else if constexpr (std::is_same_v<Tag, FenceTag>)
            return m_Fences;
        else if constexpr (std::is_same_v<Tag, SemaphoreTag>)
            return m_Semaphores;
        else if constexpr (std::is_same_v<Tag, TimelineSemaphoreTag>)
            return m_TimelineSemaphores;
        else if constexpr (std::is_same_v<Tag, DependencyInfoTag>)
            return m_DependencyInfos;
        else if constexpr (std::is_same_v<Tag, SplitBarrierTag>)
            return m_SplitBarriers;
        else
            static_assert(!sizeof(Tag), "No match for type");
        std::unreachable();
    }

    std::atomic<u64> m_AllocatedCount{0};
    std::atomic<u64> m_DeallocatedCount{0};

    ResourceContainerWithLock<SwapchainResource> m_Swapchains;
    ResourceContainerWithLock<CommandPoolResource> m_CommandPools;
    ResourceContainerWithLock<CommandBufferResource> m_CommandBuffers;
    ResourceContainerWithLock<BufferResource> m_Buffers;
    ResourceContainerWithLock<BufferArenaResource> m_BufferArenas;
    ResourceContainerWithLock<ImageResource> m_Images;
    ResourceContainerWithLock<SamplerResource> m_Samplers;
    ResourceContainerWithLock<RenderingAttachmentResource> m_RenderingAttachments;
    ResourceContainerWithLock<RenderingInfoResource> m_RenderingInfos;
    ResourceContainerWithLock<PipelineLayoutResource> m_PipelineLayouts;
    ResourceContainerWithLock<PipelineResource> m_Pipelines;
    ResourceContainerWithLock<ShaderModuleResource> m_ShaderModules;
    ResourceContainerWithLock<DescriptorsLayoutResource> m_DescriptorLayouts;
    ResourceContainerWithLock<DescriptorArenaAllocatorResource> m_DescriptorArenaAllocators;
    ResourceContainerWithLock<DescriptorsResource> m_Descriptors;
    ResourceContainerWithLock<FenceResource> m_Fences;
    ResourceContainerWithLock<SemaphoreResource> m_Semaphores;
    ResourceContainerWithLock<TimelineSemaphoreResource> m_TimelineSemaphores;
    ResourceContainerWithLock<DependencyInfoResource> m_DependencyInfos;
    ResourceContainerWithLock<SplitBarrierResource> m_SplitBarriers;

    std::vector<std::vector<CommandBuffer>> m_CommandPoolToBuffersMap;
};

class DeviceInternal
{
public:
    template <typename... Tags>
    using View = DeviceResources::View<Tags...>;

    static VmaAllocator& Allocator();

    static Swapchain CreateSwapchain(const auto& resources, SwapchainCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Swapchain swapchain);
    static void CreateSwapchainImages(const auto& resources, Swapchain swapchain);
    static void DestroySwapchainImages(const auto& resources, Swapchain swapchain);
    static u32 AcquireNextImage(const auto& resources, Swapchain swapchain,
        Fence renderFence, Semaphore presentSemaphore);
    static bool Present(const auto& resources, Swapchain swapchain, QueueKind queueKind,
        u32 imageIndex);
    static SwapchainDescription& GetSwapchainDescription(const auto& resources, Swapchain swapchain);
    static Semaphore GetSwapchainRenderSemaphore(const auto& resources, Swapchain swapchain,
        u32 imageIndex);

    static CommandPool CreateCommandPool(const auto& resources, CommandPoolCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, CommandPool commandPool);
    static void ResetPool(const auto& resources, CommandPool pool);

    static CommandBuffer CreateCommandBuffer(const auto& resources, CommandBufferCreateInfo&& createInfo);
    static void ResetCommandBuffer(const auto& resources, CommandBuffer cmd);
    static void BeginCommandBuffer(const auto& resources, CommandBuffer cmd);
    static void BeginCommandBuffer(const auto& resources, CommandBuffer cmd, CommandBufferUsage usage);
    static void EndCommandBuffer(const auto& resources, CommandBuffer cmd);
    static void SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);
    static void SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind, Fence fence);
    static void SubmitCommandBuffers(const auto& resources, Span<const CommandBuffer> cmds, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffers(const auto& resources, Span<const CommandBuffer> cmds, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);
    static void BeginCommandBufferLabel(const auto& resources, CommandBuffer cmd, std::string_view label);
    static void EndCommandBufferLabel(const auto& resources, CommandBuffer cmd);
    static ProfilerContext::Ctx CreateTracyGraphicsContext(const auto& resources, CommandBuffer cmd);
    static VkCommandBuffer GetProfilerCommandBuffer(const auto& resources, ProfilerContext* context);

    static Buffer CreateBuffer(const auto& resources, BufferCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Buffer buffer);
    static Buffer CreateStagingBuffer(const auto& resources, u64 sizeBytes);
    static void ResizeBuffer(const auto& resources, Buffer buffer, u64 newSize, CommandBuffer cmd, bool copyData);
    static void* MapBuffer(const auto& resources, Buffer buffer);
    static void UnmapBuffer(const auto& resources, Buffer buffer);
    static void SetBufferData(const auto& resources, Buffer buffer, Span<const std::byte> data, u64 offsetBytes);
    static void SetBufferData(const auto& resources, void* mappedAddress, Span<const std::byte> data, u64 offsetBytes);
    static void* GetBufferMappedAddress(const auto& resources, Buffer buffer);
    static usize GetBufferSizeBytes(const auto& resources, Buffer buffer);
    static const BufferDescription& GetBufferDescription(const auto& resources, Buffer buffer);
    static u64 GetDeviceAddress(const auto& resources, Buffer buffer);
    static Buffer AllocateBuffer(const auto& resources, const BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags allocationFlags);

    static BufferArena CreateBufferArena(const auto& resources, BufferArenaCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, BufferArena arena);
    static void ResizeBufferArenaPhysical(const auto& resources, BufferArena arena, u64 newSize, CommandBuffer cmd,
        bool copyData);
    static Buffer GetBufferArenaUnderlyingBuffer(const auto& resources, BufferArena arena);
    static u64 GetBufferArenaSizeBytesPhysical(const auto& resources, BufferArena arena);
    static BufferSuballocationResult BufferArenaSuballocate(const auto& resources, BufferArena arena, u64 sizeBytes,
        u32 alignment);
    static void BufferArenaFree(const auto& resources, BufferArena arena, BufferSuballocationHandle suballocation);

    static Image CreateImage(const auto& resources, ImageCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Image image);
    static void CreateViews(const auto& resources, const ImageSubresource& image,
        const std::vector<ImageSubresourceDescription>& additionalViews);
    static Span<const ImageSubresourceDescription> GetAdditionalImageViews(const auto& resources, Image image);
    static ImageViewHandle GetImageViewHandle(const auto& resources, Image image,
        ImageSubresourceDescription subresourceDescription);
    static const ImageDescription& GetImageDescription(const auto& resources, Image image);
    static Image CreateImageFromAssetFile(const auto& resources, ImageCreateInfo& createInfo,
        const lux::assetlib::ImageAsset* asset);
    static Image CreateImageFromPixels(const auto& resources, ImageCreateInfo& createInfo,
        Span<const std::byte> pixels);
    static Image CreateImageFromBuffer(const auto& resources, ImageCreateInfo& createInfo, Buffer buffer);
    static Image CreateEmptyImage(const auto& resources, ImageCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void PreprocessCreateInfo(ImageCreateInfo& createInfo);
    static Image AllocateImage(const auto& resources, ImageCreateInfo& createInfo);
    static VkImageView CreateVulkanImageView(const auto& resources, const ImageSubresource& image, VkFormat format);
    static ImTextureID CreateImGuiImage(const auto& resources, const ImageSubresource& texture, Sampler sampler,
        ImageLayout layout);
    static void DestroyImGuiImage(ImTextureID image);

    static Sampler CreateSampler(const auto& resources, SamplerCreateInfo&& createInfo);
    static void Destroy(const auto& resources, Sampler sampler);

    static RenderingAttachment CreateRenderingAttachment(const auto& resources,
        RenderingAttachmentCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, RenderingAttachment renderingAttachment);

    static RenderingInfo CreateRenderingInfo(const auto& resources, RenderingInfoCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, RenderingInfo renderingInfo);

    static PipelineLayout CreatePipelineLayout(const auto& resources, PipelineLayoutCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, PipelineLayout pipelineLayout);

    static Pipeline CreatePipeline(const auto& resources, PipelineCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Pipeline pipeline);

    static ShaderModule CreateShaderModule(const auto& resources, ShaderModuleCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, ShaderModule shaderModule);

    static DescriptorsLayout CreateDescriptorsLayout(const auto& resources, DescriptorsLayoutCreateInfo&& createInfo);
    static DescriptorsLayout GetEmptyDescriptorsLayout(const auto& resources);
    static void Destroy(const auto& resources, DescriptorsLayout layout);

    static DescriptorArenaAllocator CreateDescriptorArenaAllocator(const auto& resources,
        DescriptorArenaAllocatorCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, DescriptorArenaAllocator allocator);
    static std::optional<Descriptors> AllocateDescriptors(const auto& resources, DescriptorArenaAllocator allocator,
        DescriptorsLayout layout, DescriptorAllocatorAllocationBindings&& bindings);
    static void ResetDescriptorArenaAllocator(const auto& resources, DescriptorArenaAllocator allocator);

    static void UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
        const BufferSubresource& buffer, u32 index);
    static void UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
        Sampler sampler);
    static void UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
        const ImageSubresource& image, ImageLayout layout, u32 index);

    static Fence CreateFence(const auto& resources, FenceCreateInfo&& createInfo, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Fence fence);
    static void WaitForFence(const auto& resources, Fence fence);
    static bool CheckFence(const auto& resources, Fence fence);
    static void ResetFence(const auto& resources, Fence fence);

    static Semaphore CreateSemaphore(const auto& resources, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, Semaphore semaphore);

    static TimelineSemaphore CreateTimelineSemaphore(const auto& resources, TimelineSemaphoreCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, TimelineSemaphore semaphore);
    static void TimelineSemaphoreWaitCPU(const auto& resources, TimelineSemaphore semaphore, u64 value);
    static void TimelineSemaphoreSignalCPU(const auto& resources, TimelineSemaphore semaphore, u64 value);

    static DependencyInfo CreateDependencyInfo(const auto& resources, DependencyInfoCreateInfo&& createInfo,
        DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, DependencyInfo dependencyInfo);

    static SplitBarrier CreateSplitBarrier(const auto& resources, DeletionQueue& deletionQueue);
    static void Destroy(const auto& resources, SplitBarrier splitBarrier);

    static ImmediateSubmitContext StartSubmitContext(const auto& resources);
    static void EndSubmitContext(const auto& resources, const ImmediateSubmitContext& ctx);
    template <typename LockedView, typename Fn>
    static void ImmediateSubmit(LockedView& resources, Fn&& uploadFunction);

#ifdef DESCRIPTOR_BUFFER
    static u32 GetDescriptorSizeBytes(DescriptorType type);
    static void WriteDescriptor(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo, u32 index,
        VkDescriptorGetInfoEXT& descriptorGetInfo);
#else
    static u32 GetFreePoolIndexFromAllocator(const auto& resources, DescriptorArenaAllocator allocator,
        DescriptorPoolFlags poolFlags);
#endif

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(const auto& resources,
        Span<const Semaphore> semaphores, Span<const PipelineStage> waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(const auto& resources,
        Span<const TimelineSemaphore> semaphores, Span<const u64> waitValues, Span<const PipelineStage> waitStages);
    static void BindDescriptors(const auto& resources, CommandBuffer cmd, PipelineLayout pipelineLayout,
        Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const ExecuteSecondaryBufferCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const PrepareSwapchainPresentCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BeginRenderingCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const EndRenderingCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const ImGuiBeginCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const ImGuiEndCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const BeginConditionalRenderingCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const EndConditionalRenderingCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const SetViewportCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const SetScissorsCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const SetDepthBiasCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const CopyBufferCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const CopyBufferToImageCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const CopyImageCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BlitImageCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const MipmapImageCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const WaitOnFullPipelineBarrierCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const WaitOnBarrierCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const SignalSplitBarrierCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const WaitOnSplitBarrierCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const ResetSplitBarrierCommand& command);


    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindVertexBuffersCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU32BufferCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU16BufferCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU8BufferCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindPipelineGraphicsCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindPipelineComputeCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const BindImmutableSamplersGraphicsCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const BindImmutableSamplersComputeCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindDescriptorsGraphicsCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const BindDescriptorsComputeCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const BindDescriptorArenaAllocatorsCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const PushConstantsCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const DrawCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const DrawIndexedCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const DrawIndexedIndirectCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd,
        const DrawIndexedIndirectCountCommand& command);

    static void CompileCommand(const auto& resources, CommandBuffer cmd, const DispatchCommand& command);
    static void CompileCommand(const auto& resources, CommandBuffer cmd, const DispatchIndirectCommand& command);
};

DeviceCreateInfo DeviceCreateInfo::Default(lux::Window* window, bool asyncCompute)
{
    DeviceCreateInfo createInfo = {};
    createInfo.AppName = "Vulkan-app";
    createInfo.ApiVersion = VK_API_VERSION_1_3;

    const Span<const char*> instanceExtensions = lux::VulkanWindowSurface::GetRequiredExtensions();
    createInfo.InstanceExtensions.append_range(instanceExtensions);
    createInfo.InstanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#ifdef VULKAN_VAL_LAYERS
    createInfo.InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

#ifdef VULKAN_VAL_LAYERS
    createInfo.InstanceValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    createInfo.DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,
        VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME,
#ifdef DESCRIPTOR_BUFFER
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
#endif
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    createInfo.Window = window;
    createInfo.AsyncCompute = asyncCompute;

    return createInfo;
}

void DeviceResources::MapCmdToPool(CommandBuffer cmd, CommandPool pool)
{
    m_CommandPoolToBuffersMap[pool.m_Id].push_back(cmd.m_Id);
}

void DeviceResources::DestroyCmdsOfPool(CommandPool pool)
{
    for (auto cmd : m_CommandPoolToBuffersMap[pool.m_Id])
        m_CommandBuffers.Container.Erase(cmd);
    m_DeallocatedCount += (u32)m_CommandPoolToBuffersMap[pool.m_Id].size();
    m_CommandPoolToBuffersMap[pool.m_Id].clear();
}

struct QueueInfo
{
    /* technically any family index is possible;
     * practically GPUs have only a few */
    static constexpr u32 UNSET_FAMILY = std::numeric_limits<u32>::max();
    VkQueue Queue{VK_NULL_HANDLE};
    u32 Family{UNSET_FAMILY};
};

struct DeviceState
{
    struct DeviceQueues
    {
        bool IsComplete() const;

        std::vector<u32> AsFamilySet() const;

        QueueInfo GetQueueByKind(QueueKind queueKind) const;

        u32 GetFamilyByKind(QueueKind queueKind) const;

    public:
        QueueInfo Graphics;
        QueueInfo Presentation;
        QueueInfo Compute;
    };

    lux::VulkanWindowSurface& GetWindowSurface() const;

    void Shutdown();

    VkDevice Device{VK_NULL_HANDLE};
    DeviceResources Resources;
    VmaAllocator Allocator;
    DeviceQueues Queues;
    ::DeletionQueue DeletionQueue;
    ::DeletionQueue DummyDeletionQueue;

    ::DeletionQueue* FrameDeletionQueue{nullptr};

    lux::Window* Window{nullptr};
    std::unique_ptr<lux::WindowSurface> WindowSurface{};

    std::mutex SubmitContextMutex{};
    std::vector<ImmediateSubmitContext> SubmitContexts;

    VkDescriptorPool ImGuiPool;

    VkInstance Instance{VK_NULL_HANDLE};
    VkSurfaceKHR Surface{VK_NULL_HANDLE};
    VkPhysicalDevice GPU{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties GPUProperties;
    VkPhysicalDeviceDescriptorIndexingProperties GPUDescriptorIndexingProperties;
    VkPhysicalDeviceSubgroupProperties GPUSubgroupProperties;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT GPUDescriptorBufferProperties;
    VkDebugUtilsMessengerEXT DebugUtilsMessenger;
};

namespace
{
DeviceState g_State = DeviceState{};
}

bool DeviceState::DeviceQueues::IsComplete() const
{
    return
        Graphics.Family != QueueInfo::UNSET_FAMILY &&
        (Presentation.Family != QueueInfo::UNSET_FAMILY || g_State.Surface == VK_NULL_HANDLE) &&
        Compute.Family != QueueInfo::UNSET_FAMILY;
}

std::vector<u32> DeviceState::DeviceQueues::AsFamilySet() const
{
    std::vector familySet{Graphics.Family};
    if (Presentation.Family != Graphics.Family)
        familySet.push_back(Presentation.Family);
    if (Compute.Family != Graphics.Family && Compute.Family != Presentation.Family)
        familySet.push_back(Compute.Family);

    return familySet;
}

QueueInfo DeviceState::DeviceQueues::GetQueueByKind(QueueKind queueKind) const
{
    switch (queueKind)
    {
    case QueueKind::Graphics: return Graphics;
    case QueueKind::Presentation: return Presentation;
    case QueueKind::Compute: return Compute;
    default:
        ASSERT(false, "Unrecognized queue kind")
        break;
    }
    std::unreachable();
}

u32 DeviceState::DeviceQueues::GetFamilyByKind(QueueKind queueKind) const
{
    return GetQueueByKind(queueKind).Family;
}

lux::VulkanWindowSurface& DeviceState::GetWindowSurface() const
{
    return (lux::VulkanWindowSurface&)*WindowSurface;
}

void DeviceState::Shutdown()
{
    DeletionQueue.Flush();
    vmaDestroyAllocator(g_State.Allocator);
    for (auto& ctx : g_State.SubmitContexts)
    {
        Device::Destroy(ctx.CommandPool);
        Device::Destroy(ctx.Fence);
    }
    ASSERT(Resources.m_AllocatedCount == Resources.m_DeallocatedCount, "Not all driver resources are destroyed")
}

DeviceResources& deviceResources()
{
    return g_State.Resources;
}

void Device::BeginFrame(FrameContext& ctx)
{
    g_State.FrameDeletionQueue = &ctx.DeletionQueue;
}

Swapchain Device::CreateSwapchain(SwapchainCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<SwapchainTag, ImageTag, SemaphoreTag>();

    return DeviceInternal::CreateSwapchain(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(Swapchain swapchain)
{
    auto view = deviceResources().GetLockedView<SwapchainTag, ImageTag, SemaphoreTag>();

    return DeviceInternal::Destroy(view, swapchain);
}

u32 Device::AcquireNextImage(Swapchain swapchain, Fence renderFence, Semaphore presentSemaphore)
{
    auto view = deviceResources().GetLockedView<SwapchainTag, FenceTag, SemaphoreTag>();

    return DeviceInternal::AcquireNextImage(view, swapchain, renderFence, presentSemaphore);
}

bool Device::Present(Swapchain swapchain, QueueKind queueKind, u32 imageIndex)
{
    auto view = deviceResources().GetLockedView<SwapchainTag, SemaphoreTag>();

    return DeviceInternal::Present(view, swapchain, queueKind, imageIndex);
}

SwapchainDescription& Device::GetSwapchainDescription(Swapchain swapchain)
{
    auto view = deviceResources().GetLockedView<SwapchainTag>();

    return DeviceInternal::GetSwapchainDescription(view, swapchain);
}

Semaphore Device::GetSwapchainRenderSemaphore(Swapchain swapchain, u32 imageIndex)
{
    auto view = deviceResources().GetLockedView<SwapchainTag>();

    return DeviceInternal::GetSwapchainRenderSemaphore(view, swapchain, imageIndex);
}

CommandPool Device::CreateCommandPool(CommandPoolCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<CommandPoolTag>();

    return DeviceInternal::CreateCommandPool(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(CommandPool pool)
{
    auto view = deviceResources().GetLockedView<CommandPoolTag, CommandBufferTag>();
    DeviceInternal::Destroy(view, pool);
}

void Device::ResetPool(CommandPool pool)
{
    auto view = deviceResources().GetLockedView<CommandPoolTag>();
    DeviceInternal::ResetPool(view, pool);
}

CommandBuffer Device::CreateCommandBuffer(CommandBufferCreateInfo&& createInfo)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, CommandPoolTag>();

    return DeviceInternal::CreateCommandBuffer(view, std::move(createInfo));
}

void Device::ResetCommandBuffer(CommandBuffer cmd)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::ResetCommandBuffer(view, cmd);
}

void Device::BeginCommandBuffer(CommandBuffer cmd)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::BeginCommandBuffer(view, cmd);
}

void Device::BeginCommandBuffer(CommandBuffer cmd, CommandBufferUsage usage)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::BeginCommandBuffer(view, cmd, usage);
}

void Device::EndCommandBuffer(CommandBuffer cmd)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::EndCommandBuffer(view, cmd);
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind, const BufferSubmitSyncInfo& submitSync)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, FenceTag, SemaphoreTag>();
    DeviceInternal::SubmitCommandBuffer(view, cmd, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, FenceTag, TimelineSemaphoreTag>();
    DeviceInternal::SubmitCommandBuffer(view, cmd, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind, Fence fence)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, FenceTag>();
    DeviceInternal::SubmitCommandBuffer(view, cmd, queueKind, fence);
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, FenceTag, SemaphoreTag>();
    DeviceInternal::SubmitCommandBuffers(view, cmds, queueKind, submitSync);
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, FenceTag, TimelineSemaphoreTag>();
    DeviceInternal::SubmitCommandBuffers(view, cmds, queueKind, submitSync);
}

Buffer Device::CreateBuffer(BufferCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::CreateBuffer(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();
    DeviceInternal::Destroy(view, buffer);
}

Buffer Device::CreateStagingBuffer(u64 sizeBytes)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::CreateStagingBuffer(view, sizeBytes);
}

void Device::ResizeBuffer(Buffer buffer, u64 newSize, CommandBuffer cmd, bool copyData)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::ResizeBuffer(view, buffer, newSize, cmd, copyData);
}

void* Device::MapBuffer(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::MapBuffer(view, buffer);
}

void Device::UnmapBuffer(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();
    DeviceInternal::UnmapBuffer(view, buffer);
}

void Device::SetBufferData(Buffer buffer, Span<const std::byte> data, u64 offsetBytes)
{
    auto view = deviceResources().GetLockedView<BufferTag>();
    DeviceInternal::SetBufferData(view, buffer, data, offsetBytes);
}

void Device::SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    auto view = deviceResources().GetLockedView<BufferTag>();
    DeviceInternal::SetBufferData(view, mappedAddress, data, offsetBytes);
}

void* Device::GetBufferMappedAddress(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::GetBufferMappedAddress(view, buffer);
}

usize Device::GetBufferSizeBytes(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::GetBufferSizeBytes(view, buffer);
}

const BufferDescription& Device::GetBufferDescription(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::GetBufferDescription(view, buffer);
}

u64 Device::GetDeviceAddress(Buffer buffer)
{
    auto view = deviceResources().GetLockedView<BufferTag>();

    return DeviceInternal::GetDeviceAddress(view, buffer);
}

BufferArena Device::CreateBufferArena(BufferArenaCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag>();

    return DeviceInternal::CreateBufferArena(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(BufferArena bufferArena)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag>();
    DeviceInternal::Destroy(view, bufferArena);
}

void Device::ResizeBufferArenaPhysical(BufferArena arena, u64 newSize, CommandBuffer cmd, bool copyData)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferArenaTag, BufferTag>();
    DeviceInternal::ResizeBufferArenaPhysical(view, arena, newSize, cmd, copyData);
}

Buffer Device::GetBufferArenaUnderlyingBuffer(BufferArena arena)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag>();

    return DeviceInternal::GetBufferArenaUnderlyingBuffer(view, arena);
}

u64 Device::GetBufferArenaSizeBytesPhysical(BufferArena arena)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag, BufferTag>();

    return DeviceInternal::GetBufferArenaSizeBytesPhysical(view, arena);
}

BufferSuballocationResult Device::BufferArenaSuballocate(BufferArena arena, u64 sizeBytes, u32 alignment)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag, BufferTag>();

    return DeviceInternal::BufferArenaSuballocate(view, arena, sizeBytes, alignment);
}

void Device::BufferArenaFree(BufferArena arena, BufferSuballocationHandle suballocation)
{
    auto view = deviceResources().GetLockedView<BufferArenaTag>();
    DeviceInternal::BufferArenaFree(view, arena, suballocation);
}

Image Device::CreateImage(ImageCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<ImageTag, BufferTag, DependencyInfoTag, FenceTag, CommandBufferTag,
                                                CommandPoolTag>();

    return DeviceInternal::CreateImage(view, std::move(createInfo), deletionQueue);
}

Span<const ImageSubresourceDescription> Device::GetAdditionalImageViews(Image image)
{
    auto view = deviceResources().GetLockedView<ImageTag>();

    return DeviceInternal::GetAdditionalImageViews(view, image);
}

void Device::Destroy(Image image)
{
    auto view = deviceResources().GetLockedView<ImageTag>();
    DeviceInternal::Destroy(view, image);
}

void Device::CreateViews(const ImageSubresource& image, const std::vector<ImageSubresourceDescription>& additionalViews)
{
    auto view = deviceResources().GetLockedView<ImageTag>();
    DeviceInternal::CreateViews(view, image, additionalViews);
}

ImageViewHandle Device::GetImageViewHandle(Image image, ImageSubresourceDescription subresourceDescription)
{
    auto view = deviceResources().GetLockedView<ImageTag>();

    return DeviceInternal::GetImageViewHandle(view, image, subresourceDescription);
}

const ImageDescription& Device::GetImageDescription(Image image)
{
    auto view = deviceResources().GetLockedView<ImageTag>();

    return DeviceInternal::GetImageDescription(view, image);
}

Sampler Device::CreateSampler(SamplerCreateInfo&& createInfo)
{
    auto view = deviceResources().GetLockedView<SamplerTag>();

    return DeviceInternal::CreateSampler(view, std::move(createInfo));
}

void Device::Destroy(Sampler sampler)
{
    auto view = deviceResources().GetLockedView<SamplerTag>();

    DeviceInternal::Destroy(view, sampler);
}

RenderingAttachment Device::CreateRenderingAttachment(RenderingAttachmentCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<RenderingAttachmentTag, ImageTag>();

    return DeviceInternal::CreateRenderingAttachment(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(RenderingAttachment renderingAttachment)
{
    auto view = deviceResources().GetLockedView<RenderingAttachmentTag>();
    DeviceInternal::Destroy(view, renderingAttachment);
}

RenderingInfo Device::CreateRenderingInfo(RenderingInfoCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<RenderingInfoTag, RenderingAttachmentTag>();

    return DeviceInternal::CreateRenderingInfo(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(RenderingInfo renderingInfo)
{
    auto view = deviceResources().GetLockedView<RenderingInfoTag>();
    DeviceInternal::Destroy(view, renderingInfo);
}

PipelineLayout Device::CreatePipelineLayout(PipelineLayoutCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<PipelineLayoutTag, DescriptorsLayoutTag>();

    return DeviceInternal::CreatePipelineLayout(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(PipelineLayout pipelineLayout)
{
    auto view = deviceResources().GetLockedView<PipelineLayoutTag>();
    DeviceInternal::Destroy(view, pipelineLayout);
}

Pipeline Device::CreatePipeline(PipelineCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<PipelineTag, PipelineLayoutTag, ShaderModuleTag>();

    return DeviceInternal::CreatePipeline(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(Pipeline pipeline)
{
    auto view = deviceResources().GetLockedView<PipelineTag>();
    DeviceInternal::Destroy(view, pipeline);
}

ShaderModule Device::CreateShaderModule(ShaderModuleCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<ShaderModuleTag>();

    return DeviceInternal::CreateShaderModule(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(ShaderModule shaderModule)
{
    auto view = deviceResources().GetLockedView<ShaderModuleTag>();
    DeviceInternal::Destroy(view, shaderModule);
}

DescriptorsLayout Device::CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo)
{
    auto view = deviceResources().GetLockedView<DescriptorsLayoutTag, SamplerTag>();

    return DeviceInternal::CreateDescriptorsLayout(view, std::move(createInfo));
}

DescriptorsLayout Device::GetEmptyDescriptorsLayout()
{
    auto view = deviceResources().GetLockedView<DescriptorsLayoutTag, SamplerTag>();

    return DeviceInternal::GetEmptyDescriptorsLayout(view);
}

void Device::Destroy(DescriptorsLayout layout)
{
    auto view = deviceResources().GetLockedView<DescriptorsLayoutTag>();
    DeviceInternal::Destroy(view, layout);
}

DescriptorArenaAllocator Device::CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<DescriptorArenaAllocatorTag, BufferTag>();

    return DeviceInternal::CreateDescriptorArenaAllocator(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(DescriptorArenaAllocator allocator)
{
    auto view = deviceResources().GetLockedView<DescriptorArenaAllocatorTag, DescriptorsTag, BufferTag>();
    DeviceInternal::Destroy(view, allocator);
}

std::optional<Descriptors> Device::AllocateDescriptors(DescriptorArenaAllocator allocator, DescriptorsLayout layout,
    DescriptorAllocatorAllocationBindings&& bindings)
{
    auto view = deviceResources().GetLockedView<DescriptorArenaAllocatorTag, DescriptorsLayoutTag, DescriptorsTag>();

    return DeviceInternal::AllocateDescriptors(view, allocator, layout, std::move(bindings));
}

void Device::ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator)
{
    auto view = deviceResources().GetLockedView<DescriptorArenaAllocatorTag, DescriptorsTag>();
    DeviceInternal::ResetDescriptorArenaAllocator(view, allocator);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto view = deviceResources().GetLockedView<DescriptorsTag, DescriptorArenaAllocatorTag, BufferTag>();
    DeviceInternal::UpdateDescriptors(view, descriptors, slotInfo, buffer, index);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler)
{
    auto view = deviceResources().GetLockedView<DescriptorsTag, DescriptorArenaAllocatorTag, SamplerTag>();
    DeviceInternal::UpdateDescriptors(view, descriptors, slotInfo, sampler);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto view = deviceResources().GetLockedView<DescriptorsTag, DescriptorArenaAllocatorTag, ImageTag>();
    DeviceInternal::UpdateDescriptors(view, descriptors, slotInfo, image, layout, index);
}

Fence Device::CreateFence(FenceCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<FenceTag>();

    return DeviceInternal::CreateFence(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(Fence fence)
{
    auto view = deviceResources().GetLockedView<FenceTag>();
    DeviceInternal::Destroy(view, fence);
}

void Device::WaitForFence(Fence fence)
{
    auto view = deviceResources().GetLockedView<FenceTag>();
    DeviceInternal::WaitForFence(view, fence);
}

bool Device::CheckFence(Fence fence)
{
    auto view = deviceResources().GetLockedView<FenceTag>();

    return DeviceInternal::CheckFence(view, fence);
}

void Device::ResetFence(Fence fence)
{
    auto view = deviceResources().GetLockedView<FenceTag>();
    DeviceInternal::ResetFence(view, fence);
}

Semaphore Device::CreateSemaphore(::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<SemaphoreTag>();

    return DeviceInternal::CreateSemaphore(view, deletionQueue);
}

void Device::Destroy(Semaphore semaphore)
{
    auto view = deviceResources().GetLockedView<SemaphoreTag>();
    DeviceInternal::Destroy(view, semaphore);
}

TimelineSemaphore Device::CreateTimelineSemaphore(TimelineSemaphoreCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<TimelineSemaphoreTag>();

    return DeviceInternal::CreateTimelineSemaphore(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(TimelineSemaphore semaphore)
{
    auto view = deviceResources().GetLockedView<TimelineSemaphoreTag>();
    DeviceInternal::Destroy(view, semaphore);
}

void Device::TimelineSemaphoreWaitCPU(TimelineSemaphore semaphore, u64 value)
{
    auto view = deviceResources().GetLockedView<TimelineSemaphoreTag>();
    DeviceInternal::TimelineSemaphoreWaitCPU(view, semaphore, value);
}

void Device::TimelineSemaphoreSignalCPU(TimelineSemaphore semaphore, u64 value)
{
    auto view = deviceResources().GetLockedView<TimelineSemaphoreTag>();
    DeviceInternal::TimelineSemaphoreSignalCPU(view, semaphore, value);
}

DependencyInfo Device::CreateDependencyInfo(DependencyInfoCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<DependencyInfoTag, ImageTag>();

    return DeviceInternal::CreateDependencyInfo(view, std::move(createInfo), deletionQueue);
}

void Device::Destroy(DependencyInfo dependencyInfo)
{
    auto view = deviceResources().GetLockedView<DependencyInfoTag>();
    DeviceInternal::Destroy(view, dependencyInfo);
}

SplitBarrier Device::CreateSplitBarrier(::DeletionQueue& deletionQueue)
{
    auto view = deviceResources().GetLockedView<SplitBarrierTag>();

    return DeviceInternal::CreateSplitBarrier(view, deletionQueue);
}

void Device::Destroy(SplitBarrier splitBarrier)
{
    auto view = deviceResources().GetLockedView<SplitBarrierTag>();
    DeviceInternal::Destroy(view, splitBarrier);
}

void Device::CreateInstance(const DeviceCreateInfo& createInfo)
{
    auto checkInstanceExtensions = [](const DeviceCreateInfo& createInfo)
    {
        u32 availableExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

        return utils::checkArrayContainsSubArray(createInfo.InstanceExtensions, availableExtensions,
            [](const char* req, const VkExtensionProperties& avail)
            {
                return std::strcmp(req, avail.extensionName) == 0;
            },
            [](const char* req) { LUX_LOG_ERROR("Unsupported instance extension: {}\n", req); });
    };
#ifdef VULKAN_VAL_LAYERS
    auto checkInstanceValidationLayers = [](const DeviceCreateInfo& createInfo)
    {
        u32 availableValidationLayerCount = 0;
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(availableValidationLayerCount);
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, availableLayers.data());

        return utils::checkArrayContainsSubArray(createInfo.InstanceValidationLayers, availableLayers,
            [](const char* req, const VkLayerProperties& avail) { return std::strcmp(req, avail.layerName) == 0; },
            [](const char* req) { LUX_LOG_ERROR("Unsupported validation layer: {}\n", req); });
    };
#endif

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = createInfo.AppName.data();
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.pEngineName = "No engine";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.apiVersion = createInfo.ApiVersion;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    bool isEveryExtensionSupported = checkInstanceExtensions(createInfo);
    instanceCreateInfo.enabledExtensionCount = (u32)createInfo.InstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = createInfo.InstanceExtensions.data();
#ifdef VULKAN_VAL_LAYERS
    bool isEveryValidationLayerSupported = checkInstanceValidationLayers(createInfo);
    instanceCreateInfo.enabledLayerCount = (u32)createInfo.InstanceValidationLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = createInfo.InstanceValidationLayers.data();
#else
    bool isEveryValidationLayerSupported = true;
    instanceCreateInfo.enabledLayerCount = 0;
#endif
    ASSERT(isEveryExtensionSupported && isEveryValidationLayerSupported,
        "Failed to create instance")
    deviceCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &g_State.Instance),
        "Failed to create instance\n");

    volkLoadInstance(g_State.Instance);
}

void Device::CreateSurface(const DeviceCreateInfo& createInfo)
{
    if (createInfo.Window == nullptr)
    {
        LUX_LOG_WARN("Running Vulkan without swapchain: window pointer is unset");
        return;
    }

    g_State.Window = createInfo.Window;
    g_State.WindowSurface = createInfo.Window->CreateSurfaceFor(lux::WindowSurfaceBackend::Vulkan);
    void* opaqueSurface = g_State.Surface;
    ASSERT(g_State.GetWindowSurface().Init(g_State.Instance, &opaqueSurface), "Failed to create surface\n")
    g_State.Surface = (VkSurfaceKHR)opaqueSurface;
}

void Device::ChooseGPU(const DeviceCreateInfo& createInfo)
{
    auto findQueueFamilies = [](VkPhysicalDevice gpu, bool dedicatedCompute)
    {
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

        DeviceState::DeviceQueues queues = {};

        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                queues.Graphics.Family == QueueInfo::UNSET_FAMILY)
                queues.Graphics.Family = i;

            if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && queues.Compute.Family == QueueInfo::UNSET_FAMILY)
                if (!dedicatedCompute || !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    queues.Compute.Family = i;

            if (g_State.Surface != VK_NULL_HANDLE)
            {
                VkBool32 isPresentationSupported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, g_State.Surface, &isPresentationSupported);
                if (isPresentationSupported && queues.Presentation.Family == QueueInfo::UNSET_FAMILY)
                    queues.Presentation.Family = i;
            }

            if (queues.IsComplete())
                break;
        }

        return queues;
    };

    auto isGPUSuitable = [&findQueueFamilies](VkPhysicalDevice gpu,
        const DeviceCreateInfo& createInfo)
    {
        auto checkGPUExtensions = [](VkPhysicalDevice gpu, const DeviceCreateInfo& createInfo)
        {
            u32 availableExtensionCount = 0;
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, availableExtensions.data());

            return utils::checkArrayContainsSubArray(createInfo.DeviceExtensions, availableExtensions,
                [](const char* req, const VkExtensionProperties& avail)
                {
                    return std::strcmp(req, avail.extensionName) == 0;
                },
                [](const char* req) { LUX_LOG_ERROR("Unsupported device extension: {}\n", req); });
        };

        auto checkGPUFeatures = [](VkPhysicalDevice gpu)
        {
            VkPhysicalDeviceShaderDrawParametersFeatures shaderFeatures = {};
            shaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;

            VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
            descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptorIndexingFeatures.pNext = &shaderFeatures;

            VkPhysicalDeviceVulkan11Features deviceVulkan11Features = {};
            deviceVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            deviceVulkan11Features.pNext = &descriptorIndexingFeatures;

            VkPhysicalDeviceVulkan12Features deviceVulkan12Features = {};
            deviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            deviceVulkan12Features.pNext = &deviceVulkan11Features;

            VkPhysicalDeviceVulkan13Features deviceVulkan13Features = {};
            deviceVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            deviceVulkan13Features.pNext = &deviceVulkan12Features;

            VkPhysicalDeviceConditionalRenderingFeaturesEXT conditionalRenderingFeaturesExt = {};
            conditionalRenderingFeaturesExt.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
            conditionalRenderingFeaturesExt.pNext = &deviceVulkan13Features;

            VkPhysicalDeviceIndexTypeUint8FeaturesEXT physicalDeviceIndexTypeUint8FeaturesExt = {};
            physicalDeviceIndexTypeUint8FeaturesExt.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
            physicalDeviceIndexTypeUint8FeaturesExt.pNext = &conditionalRenderingFeaturesExt;

            VkPhysicalDeviceDescriptorBufferFeaturesEXT physicalDeviceDescriptorBufferFeaturesExt = {};
            physicalDeviceDescriptorBufferFeaturesExt.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
            physicalDeviceDescriptorBufferFeaturesExt.pNext = &physicalDeviceIndexTypeUint8FeaturesExt;

            VkPhysicalDeviceFeatures2 features = {};
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features.pNext = &physicalDeviceDescriptorBufferFeaturesExt;

            vkGetPhysicalDeviceFeatures2(gpu, &features);

            bool suitable = features.features.samplerAnisotropy == VK_TRUE &&
                features.features.multiDrawIndirect == VK_TRUE &&
                features.features.drawIndirectFirstInstance == VK_TRUE &&
                features.features.geometryShader == VK_TRUE &&
                features.features.shaderSampledImageArrayDynamicIndexing == VK_TRUE &&
                features.features.shaderInt16 == VK_TRUE &&
                features.features.shaderInt64 == VK_TRUE &&
                features.features.depthClamp == VK_TRUE &&
                descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE &&
                descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE &&
                descriptorIndexingFeatures.descriptorBindingPartiallyBound == VK_TRUE &&
                descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending == VK_TRUE &&
                descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount == VK_TRUE &&
                descriptorIndexingFeatures.runtimeDescriptorArray == VK_TRUE &&
                shaderFeatures.shaderDrawParameters == VK_TRUE &&
                deviceVulkan11Features.storageBuffer16BitAccess == VK_TRUE &&
                deviceVulkan11Features.uniformAndStorageBuffer16BitAccess == VK_TRUE &&
                deviceVulkan12Features.samplerFilterMinmax == VK_TRUE &&
                deviceVulkan12Features.drawIndirectCount == VK_TRUE &&
                deviceVulkan12Features.subgroupBroadcastDynamicId == VK_TRUE &&
                deviceVulkan12Features.shaderFloat16 == VK_TRUE &&
                deviceVulkan12Features.shaderInt8 == VK_TRUE &&
                deviceVulkan12Features.storageBuffer8BitAccess == VK_TRUE &&
                deviceVulkan12Features.uniformAndStorageBuffer8BitAccess == VK_TRUE &&
                deviceVulkan12Features.shaderSubgroupExtendedTypes == VK_TRUE &&
                deviceVulkan12Features.shaderBufferInt64Atomics == VK_TRUE &&
                deviceVulkan12Features.timelineSemaphore == VK_TRUE &&
                deviceVulkan12Features.bufferDeviceAddress == VK_TRUE &&
                deviceVulkan12Features.scalarBlockLayout == VK_TRUE &&
                deviceVulkan13Features.dynamicRendering == VK_TRUE &&
                deviceVulkan13Features.synchronization2 == VK_TRUE &&
                deviceVulkan13Features.shaderDemoteToHelperInvocation == VK_TRUE &&
                conditionalRenderingFeaturesExt.conditionalRendering == VK_TRUE &&
                physicalDeviceIndexTypeUint8FeaturesExt.indexTypeUint8 == VK_TRUE;
#ifdef DESCRIPTOR_BUFFER
            suitable = suitable && physicalDeviceDescriptorBufferFeaturesExt.descriptorBuffer == VK_TRUE;
#endif

            return suitable;
        };

        DeviceState::DeviceQueues deviceQueues = findQueueFamilies(gpu, createInfo.AsyncCompute);
        if (!deviceQueues.IsComplete())
            return false;

        bool isEveryExtensionSupported = checkGPUExtensions(gpu, createInfo);
        if (!isEveryExtensionSupported)
            return false;

        if (g_State.Surface != VK_NULL_HANDLE)
        {
            SurfaceDetails surfaceDetails = getSurfaceDetails(gpu, g_State.Surface);
            if (!surfaceDetails.IsSufficient())
                return false;
        }

        bool isEveryFeatureSupported = checkGPUFeatures(gpu);
        if (!isEveryFeatureSupported)
            return false;

        return true;
    };

    u32 availableGPUCount = 0;
    vkEnumeratePhysicalDevices(g_State.Instance, &availableGPUCount, nullptr);
    std::vector<VkPhysicalDevice> availableGPUs(availableGPUCount);
    vkEnumeratePhysicalDevices(g_State.Instance, &availableGPUCount, availableGPUs.data());

    for (auto candidate : availableGPUs)
    {
        if (isGPUSuitable(candidate, createInfo))
        {
            g_State.GPU = candidate;
            g_State.Queues = findQueueFamilies(candidate, createInfo.AsyncCompute);
            break;
        }
    }

    ASSERT(g_State.GPU != VK_NULL_HANDLE, "Failed to find suitable gpu device")

    g_State.GPUDescriptorIndexingProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    g_State.GPUSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    g_State.GPUSubgroupProperties.pNext = &g_State.GPUDescriptorIndexingProperties;

    g_State.GPUDescriptorBufferProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    g_State.GPUDescriptorBufferProperties.pNext = &g_State.GPUSubgroupProperties;

    VkPhysicalDeviceProperties2 deviceProperties2 = {};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &g_State.GPUDescriptorBufferProperties;
    vkGetPhysicalDeviceProperties2(g_State.GPU, &deviceProperties2);
    g_State.GPUProperties = deviceProperties2.properties;
}

void Device::CreateDevice(const DeviceCreateInfo& createInfo)
{
    std::vector<u32> queueFamilies = g_State.Queues.AsFamilySet();
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(queueFamilies.size());
    f32 queuePriority = 1.0f;
    for (u32 i = 0; i < queueFamilies.size(); i++)
    {
        if (queueFamilies[i] == QueueInfo::UNSET_FAMILY)
            continue;
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;
    vulkan11Features.uniformAndStorageBuffer16BitAccess = VK_TRUE;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    deviceFeatures.shaderInt16 = VK_TRUE;
    deviceFeatures.shaderInt64 = VK_TRUE;
    deviceFeatures.depthClamp = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.pNext = &vulkan11Features;
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.samplerFilterMinmax = VK_TRUE;
    vulkan12Features.drawIndirectCount = VK_TRUE;
    vulkan12Features.subgroupBroadcastDynamicId = VK_TRUE;
    vulkan12Features.shaderFloat16 = VK_TRUE;
    vulkan12Features.shaderInt8 = VK_TRUE;
    vulkan12Features.storageBuffer8BitAccess = VK_TRUE;
    vulkan12Features.shaderSubgroupExtendedTypes = VK_TRUE;
    vulkan12Features.shaderBufferInt64Atomics = VK_TRUE;
    vulkan12Features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    vulkan12Features.timelineSemaphore = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.scalarBlockLayout = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &vulkan12Features;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.shaderDemoteToHelperInvocation = VK_TRUE;

    VkPhysicalDeviceConditionalRenderingFeaturesEXT conditionalRenderingFeaturesExt = {};
    conditionalRenderingFeaturesExt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
    conditionalRenderingFeaturesExt.pNext = &vulkan13Features;
    conditionalRenderingFeaturesExt.conditionalRendering = VK_TRUE;

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT physicalDeviceIndexTypeUint8FeaturesExt = {};
    physicalDeviceIndexTypeUint8FeaturesExt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
    physicalDeviceIndexTypeUint8FeaturesExt.pNext = &conditionalRenderingFeaturesExt;
    physicalDeviceIndexTypeUint8FeaturesExt.indexTypeUint8 = VK_TRUE;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT physicalDeviceDescriptorBufferFeaturesExt = {};
    physicalDeviceDescriptorBufferFeaturesExt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    physicalDeviceDescriptorBufferFeaturesExt.pNext = &physicalDeviceIndexTypeUint8FeaturesExt;
#ifdef DESCRIPTOR_BUFFER
    physicalDeviceDescriptorBufferFeaturesExt.descriptorBuffer = VK_TRUE;
#endif

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &physicalDeviceDescriptorBufferFeaturesExt;
    deviceCreateInfo.queueCreateInfoCount = (u32)queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = (u32)createInfo.DeviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = createInfo.DeviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    deviceCheck(vkCreateDevice(g_State.GPU, &deviceCreateInfo, nullptr, &g_State.Device),
        "Failed to create device\n");

    volkLoadDevice(g_State.Device);
}

void Device::RetrieveDeviceQueues()
{
    g_State.Queues.Graphics.Queue = {};
    g_State.Queues.Presentation.Queue = {};
    g_State.Queues.Compute.Queue = {};

    vkGetDeviceQueue(g_State.Device, g_State.Queues.Graphics.Family, 0, &g_State.Queues.Graphics.Queue);
    if (g_State.Surface != VK_NULL_HANDLE)
        vkGetDeviceQueue(g_State.Device, g_State.Queues.Presentation.Family, 0, &g_State.Queues.Presentation.Queue);
    vkGetDeviceQueue(g_State.Device, g_State.Queues.Compute.Family, 0, &g_State.Queues.Compute.Queue);
}

namespace
{
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData)
{
    LUX_LOG_ERROR("VALIDATION LAYER: {}", callbackData->pMessage);
    return VK_FALSE;
}
}

void Device::CreateDebugUtilsMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugCallback;
    vkCreateDebugUtilsMessengerEXT(
        g_State.Instance, &debugUtilsMessengerCreateInfo, nullptr, &g_State.DebugUtilsMessenger);
}

void Device::DestroyDebugUtilsMessenger()
{
    vkDestroyDebugUtilsMessengerEXT(g_State.Instance, g_State.DebugUtilsMessenger, nullptr);
}

void Device::WaitIdle()
{
    vkDeviceWaitIdle(g_State.Device);
}

void Device::Init(DeviceCreateInfo&& createInfo)
{
    deviceCheck(volkInitialize(), "Failed to initialize volk");

    CreateInstance(createInfo);
    CreateSurface(createInfo);
    ChooseGPU(createInfo);
    CreateDevice(createInfo);
    RetrieveDeviceQueues();

#ifdef VULKAN_VAL_LAYERS
    CreateDebugUtilsMessenger();
#endif

    VmaVulkanFunctions vulkanFunctions;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.instance = g_State.Instance;
    vmaCreateInfo.physicalDevice = g_State.GPU;
    vmaCreateInfo.device = g_State.Device;
    vmaCreateInfo.pVulkanFunctions = (const VmaVulkanFunctions*)&vulkanFunctions;
    vmaCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&vmaCreateInfo, &g_State.Allocator);

    g_State.DummyDeletionQueue.m_IsDummy = true;

    if (g_State.Surface != VK_NULL_HANDLE)
        InitImGuiUI();
}

void Device::Shutdown()
{
    vkDeviceWaitIdle(g_State.Device);

    ShutdownImGuiUI();
    g_State.Shutdown();

#ifdef VULKAN_VAL_LAYERS
    DestroyDebugUtilsMessenger();
#endif

    vkDestroyDevice(g_State.Device, nullptr);
    vkDestroySurfaceKHR(g_State.Instance, g_State.Surface, nullptr);
    vkDestroyInstance(g_State.Instance, nullptr);
}

DeletionQueue& Device::DeletionQueue()
{
    return g_State.DeletionQueue;
}

DeletionQueue& Device::DummyDeletionQueue()
{
    return g_State.DummyDeletionQueue;
}

u64 Device::GetUniformBufferAlignment()
{
    return g_State.GPUProperties.limits.minUniformBufferOffsetAlignment;
}

f32 Device::GetAnisotropyLevel()
{
    return g_State.GPUProperties.limits.maxSamplerAnisotropy;
}

u32 Device::GetMaxIndexingImages()
{
    return g_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
}

u32 Device::GetMaxIndexingUniformBuffers()
{
    return g_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
}

u32 Device::GetMaxIndexingUniformBuffersDynamic()
{
    return g_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
}

u32 Device::GetMaxIndexingStorageBuffers()
{
    return g_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
}

u32 Device::GetMaxIndexingStorageBuffersDynamic()
{
    return g_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
}

u32 Device::GetSubgroupSize()
{
    return g_State.GPUSubgroupProperties.subgroupSize;
}

ImmediateSubmitContext Device::StartSubmitContext()
{
    auto view = deviceResources().GetLockedView<FenceTag, CommandBufferTag, CommandPoolTag>();
    auto ctx = DeviceInternal::StartSubmitContext(view);

    return ctx;
}

void Device::EndSubmitContext(const ImmediateSubmitContext& ctx)
{
    auto view = deviceResources().GetLockedView<FenceTag, CommandBufferTag, CommandPoolTag>();
    DeviceInternal::EndSubmitContext(view, ctx);
}

void Device::InitImGuiUI()
{
    static constexpr std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = (u32)poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(g_State.Device, &poolCreateInfo, nullptr, &g_State.ImGuiPool);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    g_State.Window->InitForImGui();

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = g_State.Instance;
    imguiInitInfo.PhysicalDevice = g_State.GPU;
    imguiInitInfo.Device = g_State.Device;
    imguiInitInfo.QueueFamily = g_State.Queues.Graphics.Family;
    imguiInitInfo.Queue = g_State.Queues.Graphics.Queue;
    imguiInitInfo.DescriptorPool = g_State.ImGuiPool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.PipelineRenderingCreateInfo = {};
    imguiInitInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
    ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* instance)
    {
        return vkGetInstanceProcAddr((VkInstance)instance, functionName);
    }, g_State.Instance);
    ImGui_ImplVulkan_Init(&imguiInitInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void Device::ShutdownImGuiUI()
{
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        ImGuiUI::ClearFrameResources(i);

    ImGui_ImplVulkan_Shutdown();
    g_State.Window->ShutdownImGui();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(g_State.Device, g_State.ImGuiPool, nullptr);
}

ProfilerContext::Ctx Device::CreateTracyGraphicsContext(CommandBuffer cmd)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();

    return DeviceInternal::CreateTracyGraphicsContext(view, cmd);
}

void Device::DestroyTracyGraphicsContext(ProfilerContext::Ctx context)
{
    TracyVkDestroy((TracyVkCtx)context)
}

void Device::CreateGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu, const SourceLocationData& sourceLocationData)
{
    static_assert(sizeof(zoneGpu.Impl) >= sizeof(tracy::VkCtxScope));

    auto view = deviceResources().GetLockedView<CommandBufferTag>();

    new(&zoneGpu.Impl) tracy::VkCtxScope(
        (TracyVkCtx)ProfilerContext::Get()->GraphicsContext(),
        (const tracy::SourceLocationData*)&sourceLocationData,
        DeviceInternal::GetProfilerCommandBuffer(view, ProfilerContext::Get()), true);
}

void Device::DestroyGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu)
{
    std::launder((tracy::VkCtxScope*)&zoneGpu.Impl)->~VkCtxScope();
}

void Device::CollectGpuProfileFrames()
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();

    TracyVkCollect(
        ((TracyVkCtx)ProfilerContext::Get()->GraphicsContext()),
        DeviceInternal::GetProfilerCommandBuffer(view, ProfilerContext::Get()))
}

ImTextureID Device::CreateImGuiImage(const ImageSubresource& texture, Sampler sampler, ImageLayout layout)
{
    auto view = deviceResources().GetLockedView<ImageTag, SamplerTag>();

    return DeviceInternal::CreateImGuiImage(view, texture, sampler, layout);
}

void Device::DestroyImGuiImage(ImTextureID image)
{
    DeviceInternal::DestroyImGuiImage(image);
}

void Device::DumpMemoryStats(const std::filesystem::path& path)
{
    static constexpr VkBool32 DETAILED_MAP = true;
    char* statsString = nullptr;
    vmaBuildStatsString(g_State.Allocator, &statsString, DETAILED_MAP);
    if (!exists(path.parent_path()))
        create_directories(path.parent_path());
    std::ofstream out(path);
    std::print(out, "{}", statsString);
    vmaFreeStatsString(g_State.Allocator, statsString);
}

void Device::BeginCommandBufferLabel(CommandBuffer cmd, std::string_view label)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::BeginCommandBufferLabel(view, cmd, label);
}

void Device::EndCommandBufferLabel(CommandBuffer cmd)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::EndCommandBufferLabel(view, cmd);
}

namespace
{
void nameVulkanHandle(VkDevice device, u64 handle, VkObjectType type, std::string_view name)
{
#ifdef VULKAN_VAL_LAYERS
    VkDebugUtilsObjectNameInfoEXT bufferName = {};
    bufferName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    bufferName.objectHandle = handle;
    bufferName.objectType = type;
    bufferName.pObjectName = name.data();
    vkSetDebugUtilsObjectNameEXT(device, &bufferName);
#endif
}
}

void Device::NameBuffer(Buffer buffer, std::string_view name)
{
    auto view = deviceResources().GetLockedView<BufferTag>();
    nameVulkanHandle(g_State.Device, (u64)view[buffer].Buffer, VK_OBJECT_TYPE_BUFFER, name);
}

void Device::NameImage(Image image, std::string_view name)
{
    auto view = deviceResources().GetLockedView<ImageTag>();
    nameVulkanHandle(g_State.Device, (u64)view[image].Image, VK_OBJECT_TYPE_IMAGE, name);
}

void Device::NamePipeline(Pipeline pipeline, std::string_view name)
{
    auto view = deviceResources().GetLockedView<PipelineTag>();
    nameVulkanHandle(g_State.Device, (u64)view[pipeline].Pipeline, VK_OBJECT_TYPE_PIPELINE, name);
}

void Device::CompileCommand(CommandBuffer cmd, const ExecuteSecondaryBufferCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const PrepareSwapchainPresentCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, SwapchainTag, DependencyInfoTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BeginRenderingCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, RenderingInfoTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const EndRenderingCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const ImGuiBeginCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const ImGuiEndCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, RenderingInfoTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BeginConditionalRenderingCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const EndConditionalRenderingCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const SetViewportCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const SetScissorsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const SetDepthBiasCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyBufferCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyBufferToImageCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyImageCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BlitImageCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const MipmapImageCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnFullPipelineBarrierCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag, ImageTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnBarrierCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const SignalSplitBarrierCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag, SplitBarrierTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnSplitBarrierCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag, SplitBarrierTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const ResetSplitBarrierCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DependencyInfoTag, SplitBarrierTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindVertexBuffersCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU32BufferCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU16BufferCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU8BufferCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindPipelineGraphicsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindPipelineComputeCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersGraphicsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineLayoutTag, DescriptorArenaAllocatorTag,
                                                DescriptorsTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersComputeCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineLayoutTag, DescriptorArenaAllocatorTag,
                                                DescriptorsTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorsGraphicsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineLayoutTag, DescriptorArenaAllocatorTag,
                                                DescriptorsTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorsComputeCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineLayoutTag, DescriptorArenaAllocatorTag,
                                                DescriptorsTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorArenaAllocatorsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, DescriptorArenaAllocatorTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const PushConstantsCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, PipelineLayoutTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCountCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DispatchCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}

void Device::CompileCommand(CommandBuffer cmd, const DispatchIndirectCommand& command)
{
    auto view = deviceResources().GetLockedView<CommandBufferTag, BufferTag>();
    DeviceInternal::CompileCommand(view, cmd, command);
}


VmaAllocator& DeviceInternal::Allocator()
{
    return g_State.Allocator;
}

Swapchain DeviceInternal::CreateSwapchain(const auto& resources, SwapchainCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    if (g_State.Surface == VK_NULL_HANDLE)
        return {};

    static std::vector<VkSurfaceFormatKHR> desiredFormats = {
        {
            {
                .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            }
        }
    };
    static std::vector<VkPresentModeKHR> desiredPresentModes = {
        {
            //VK_PRESENT_MODE_IMMEDIATE_KHR,
            VK_PRESENT_MODE_FIFO_RELAXED_KHR
        }
    };

    SurfaceDetails surfaceDetails = getSurfaceDetails(g_State.GPU, g_State.Surface);
    VkSurfaceCapabilitiesKHR capabilities = surfaceDetails.Capabilities;
    VkSurfaceFormatKHR colorFormat = utils::getIntersectionOrDefault(
        desiredFormats, surfaceDetails.Formats,
        [](VkSurfaceFormatKHR des, VkSurfaceFormatKHR avail)
        {
            return des.format == avail.format && des.colorSpace == avail.colorSpace;
        });
    VkPresentModeKHR presentMode = utils::getIntersectionOrDefault(
        desiredPresentModes, surfaceDetails.PresentModes,
        [](VkPresentModeKHR des, VkPresentModeKHR avail)
        {
            return des == avail;
        });

    auto chooseImageCount = [](const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.maxImageCount == 0)
            return capabilities.minImageCount + 1;
        return std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);
    };

    auto chooseExtent = [](const lux::Window& window, const VkSurfaceCapabilitiesKHR& capabilities)
    {
        VkExtent2D extent = capabilities.currentExtent;

        if (extent.width != std::numeric_limits<u32>::max())
            return extent;

        // indication that extent might not be same as window size
        lux::WindowSize size = window.GetWindowSize();

        extent.width = std::clamp(
            (i32)size.Width, (i32)capabilities.minImageExtent.width, (i32)capabilities.maxImageExtent.width);
        extent.height = std::clamp(
            (i32)size.Height, (i32)capabilities.minImageExtent.height, (i32)capabilities.maxImageExtent.height);

        return extent;
    };

    u32 imageCount = chooseImageCount(capabilities);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = g_State.Surface;
    swapchainCreateInfo.imageColorSpace = colorFormat.colorSpace;
    swapchainCreateInfo.imageFormat = colorFormat.format;
    VkExtent2D extent = chooseExtent(*g_State.Window, capabilities);
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.presentMode = presentMode;

    if (g_State.Queues.Graphics.Family == g_State.Queues.Presentation.Family)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        std::vector<u32> queueFamilies = g_State.Queues.AsFamilySet();
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = (u32)queueFamilies.size();
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();
    }
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    SwapchainResource swapchainResource = {};
    swapchainResource.ColorFormat = colorFormat.format;
    deviceCheck(vkCreateSwapchainKHR(g_State.Device, &swapchainCreateInfo, nullptr,
        &swapchainResource.Swapchain), "Failed to create swapchain");

    const glm::uvec2 swapchainResolution = glm::uvec2{extent.width, extent.height};
    const glm::uvec2 drawResolution = createInfo.DrawResolution.x != 0 ?
        createInfo.DrawResolution :
        swapchainResolution;

    swapchainResource.Description = {
        .SwapchainResolution = swapchainResolution,
        .DrawResolution = drawResolution,
        .DrawFormat = createInfo.DrawFormat,
        .DepthFormat = createInfo.DepthStencilFormat,
        .DrawImage = CreateEmptyImage(resources, {
                .Description = ImageDescription{
                    .Width = drawResolution.x,
                    .Height = drawResolution.y,
                    .Format = createInfo.DrawFormat,
                    .Usage = ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color
                }
            },
            Device::DummyDeletionQueue()),
        .DepthImage = CreateEmptyImage(resources, {
                .Description = ImageDescription{
                    .Width = drawResolution.x,
                    .Height = drawResolution.y,
                    .Format = createInfo.DepthStencilFormat,
                    .Usage = ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled
                }
            },
            Device::DummyDeletionQueue())
    };

    swapchainResource.RenderSemaphores.reserve(imageCount);
    for (u32 i = 0; i < imageCount; i++)
        swapchainResource.RenderSemaphores.push_back({CreateSemaphore(resources, Device::DummyDeletionQueue())});
    Swapchain swapchain = resources.Add(swapchainResource);
    CreateSwapchainImages(resources, swapchain);
    deletionQueue.Enqueue(swapchain);

    return swapchain;
}

void DeviceInternal::Destroy(const auto& resources, Swapchain swapchain)
{
    DestroySwapchainImages(resources, swapchain);
    vkDestroySwapchainKHR(g_State.Device, resources[swapchain].Swapchain, nullptr);
    for (Semaphore semaphore : resources[swapchain].RenderSemaphores)
        Destroy(resources, semaphore);
    resources.Remove(swapchain);
}

void DeviceInternal::CreateSwapchainImages(const auto& resources, Swapchain swapchain)
{
    SwapchainResource& swapchainResource = resources[swapchain];
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(g_State.Device, swapchainResource.Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(g_State.Device, swapchainResource.Swapchain, &imageCount, images.data());

    ImageDescription description = {
        .Width = swapchainResource.Description.SwapchainResolution.x,
        .Height = swapchainResource.Description.SwapchainResolution.y,
        .LayersDepth = 1,
        .Mipmaps = 1,
        .Kind = ImageKind::Image2d,
        .Usage = ImageUsage::Destination
    };
    std::vector<Image> colorImages(imageCount);

    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        ImageResource imageResource = {.Image = images[i], .Description = description};
        colorImages[i] = resources.Add(imageResource);
        resources[colorImages[i]].Views.ViewType.View = DeviceInternal::CreateVulkanImageView(resources,
            ImageSubresource{.Image = colorImages[i], .Description = {.Mipmaps = 1, .Layers = 1}},
            swapchainResource.ColorFormat);
        resources[colorImages[i]].Views.ViewList = &resources[colorImages[i]].Views.ViewType.View;
    }

    swapchainResource.Description.ColorImages = colorImages;
}

void DeviceInternal::DestroySwapchainImages(const auto& resources, Swapchain swapchain)
{
    SwapchainResource& swapchainResource = resources[swapchain];
    for (const auto& colorImage : swapchainResource.Description.ColorImages)
    {
        vkDestroyImageView(g_State.Device, *resources[colorImage].Views.ViewList, nullptr);
        resources.Remove(colorImage);
    }
    Destroy(resources, swapchainResource.Description.DrawImage);
    Destroy(resources, swapchainResource.Description.DepthImage);
}

u32 DeviceInternal::AcquireNextImage(const auto& resources, Swapchain swapchain, Fence renderFence,
    Semaphore presentSemaphore)
{
    WaitForFence(resources, renderFence);

    u32 imageIndex;
    const VkResult res = vkAcquireNextImageKHR(g_State.Device, resources[swapchain].Swapchain,
        10'000'000'000, resources[presentSemaphore].Semaphore, VK_NULL_HANDLE,
        &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        return INVALID_SWAPCHAIN_IMAGE;

    ASSERT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image")

    ResetFence(resources, renderFence);

    return imageIndex;
}

bool DeviceInternal::Present(const auto& resources, Swapchain swapchain, QueueKind queueKind, u32 imageIndex)
{
    SwapchainResource& swapchainResource = resources[swapchain];

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &resources[swapchain].Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &resources[swapchainResource.RenderSemaphores[imageIndex]].Semaphore;

    VkResult result = vkQueuePresentKHR(g_State.Queues.GetQueueByKind(queueKind).Queue, &presentInfo);

    ASSERT(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR,
        "Failed to present image")

    return result == VK_SUCCESS;
}

SwapchainDescription& DeviceInternal::GetSwapchainDescription(const auto& resources, Swapchain swapchain)
{
    return resources[swapchain].Description;
}

Semaphore DeviceInternal::GetSwapchainRenderSemaphore(const auto& resources, Swapchain swapchain, u32 imageIndex)
{
    return resources[swapchain].RenderSemaphores[imageIndex];
}

CommandPool DeviceInternal::CreateCommandPool(const auto& resources, CommandPoolCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    VkCommandPoolCreateFlags flags = 0;
    if (createInfo.PerBufferReset)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = flags;
    poolCreateInfo.queueFamilyIndex = g_State.Queues.GetFamilyByKind(createInfo.QueueKind);

    CommandPoolResource commandPoolResource = {};
    deviceCheck(vkCreateCommandPool(g_State.Device, &poolCreateInfo, nullptr, &commandPoolResource.CommandPool),
        "Failed to create command pool");

    const CommandPool commandPool = resources.Add(commandPoolResource);
    if (commandPool.m_Id >= deviceResources().m_CommandPoolToBuffersMap.size())
        deviceResources().m_CommandPoolToBuffersMap.resize(commandPool.m_Id + 1);
    deletionQueue.Enqueue(commandPool);

    return commandPool;
}

void DeviceInternal::Destroy(const auto& resources, CommandPool commandPool)
{
    vkDestroyCommandPool(g_State.Device, resources[commandPool].CommandPool, nullptr);
    deviceResources().DestroyCmdsOfPool(commandPool);
    resources.Remove(commandPool);
}

void DeviceInternal::ResetPool(const auto& resources, CommandPool pool)
{
    deviceCheck(vkResetCommandPool(g_State.Device, resources[pool].CommandPool, 0),
        "Error while resetting command pool");
}

CommandBuffer DeviceInternal::CreateCommandBuffer(const auto& resources, CommandBufferCreateInfo&& createInfo)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = resources[createInfo.Pool].CommandPool;
    allocateInfo.level = vulkanBufferLevelFromBufferKind(createInfo.Kind);
    allocateInfo.commandBufferCount = 1;

    CommandBufferResource commandBufferResource = {};
    commandBufferResource.Kind = createInfo.Kind;
    deviceCheck(vkAllocateCommandBuffers(g_State.Device, &allocateInfo, &commandBufferResource.CommandBuffer),
        "Failed to allocate command buffer");

    CommandBuffer cmd = resources.Add(commandBufferResource);
    deviceResources().MapCmdToPool(cmd, createInfo.Pool);

    return cmd;
}

void DeviceInternal::ResetCommandBuffer(const auto& resources, CommandBuffer cmd)
{
    deviceCheck(vkResetCommandBuffer(resources[cmd].CommandBuffer, 0), "Error while resetting command buffer");
}

void DeviceInternal::BeginCommandBuffer(const auto& resources, CommandBuffer cmd)
{
    BeginCommandBuffer(resources, cmd, CommandBufferUsage::SingleSubmit);
}

void DeviceInternal::BeginCommandBuffer(const auto& resources, CommandBuffer cmd, CommandBufferUsage usage)
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = vulkanCommandBufferFlagsFromUsage(usage);
    CommandBufferResource& commandBufferResource = resources[cmd];
    if (commandBufferResource.Kind == CommandBufferKind::Secondary)
        beginInfo.pInheritanceInfo = &inheritanceInfo;

    deviceCheck(vkBeginCommandBuffer(resources[cmd].CommandBuffer, &beginInfo),
        "Error while beginning command buffer");
}

void DeviceInternal::EndCommandBuffer(const auto& resources, CommandBuffer cmd)
{
    deviceCheck(vkEndCommandBuffer(resources[cmd].CommandBuffer), "Error while ending command buffer");
}

void DeviceInternal::SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    SubmitCommandBuffers(resources, {cmd}, queueKind, submitSync);
}

void DeviceInternal::SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    SubmitCommandBuffers(resources, {cmd}, queueKind, submitSync);
}

void DeviceInternal::SubmitCommandBuffer(const auto& resources, CommandBuffer cmd, QueueKind queueKind, Fence fence)
{
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo.commandBuffer = resources[cmd].CommandBuffer;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            fence.HasValue() ? resources[fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffer");
}

void DeviceInternal::SubmitCommandBuffers(const auto& resources, Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = resources[cmd].CommandBuffer;
    }

    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = resources[semaphore].Semaphore;
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = CreateVulkanSemaphoreSubmit(
        resources, submitSync.WaitSemaphores, submitSync.WaitStages);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            submitSync.Fence.HasValue() ? resources[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

void DeviceInternal::SubmitCommandBuffers(const auto& resources,
    Span<const CommandBuffer> cmds, QueueKind queueKind, const BufferSubmitTimelineSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        resources[submitSync.SignalSemaphores[i]].Timeline = submitSync.SignalValues[i];

    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = resources[cmd].CommandBuffer;
    }

    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = resources[semaphore].Semaphore;
        signalSemaphoreSubmitInfos[i].value = submitSync.SignalValues[i];
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = CreateVulkanSemaphoreSubmit(
        resources, submitSync.WaitSemaphores, submitSync.WaitValues, submitSync.WaitStages);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            submitSync.Fence.HasValue() ? resources[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

void DeviceInternal::BeginCommandBufferLabel(const auto& resources, CommandBuffer cmd, std::string_view label)
{
#ifdef VULKAN_VAL_LAYERS
    VkDebugUtilsLabelEXT debugLabel = {};
    debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debugLabel.pLabelName = label.data();
    vkCmdBeginDebugUtilsLabelEXT(resources[cmd].CommandBuffer, &debugLabel);
#endif // VULKAN_VAL_LAYERS
}

void DeviceInternal::EndCommandBufferLabel(const auto& resources, CommandBuffer cmd)
{
#ifdef VULKAN_VAL_LAYERS
    vkCmdEndDebugUtilsLabelEXT(resources[cmd].CommandBuffer);
#endif // VULKAN_VAL_LAYERS
}

ProfilerContext::Ctx DeviceInternal::CreateTracyGraphicsContext(const auto& resources, CommandBuffer cmd)
{
    const TracyVkCtx context = TracyVkContext(g_State.GPU, g_State.Device,
        g_State.Queues.Graphics.Queue, resources[cmd].CommandBuffer)

    return context;
}

VkCommandBuffer DeviceInternal::GetProfilerCommandBuffer(const auto& resources,
    ProfilerContext* context)
{
    return resources[context->m_GraphicsCommandBuffers[context->m_CurrentFrame]].CommandBuffer;
}

Buffer DeviceInternal::CreateBuffer(const auto& resources, BufferCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    VmaAllocationCreateFlags flags = 0;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::Mappable))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::MappableRandomAccess))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    if (createInfo.PersistentMapping)
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    const Buffer buffer = AllocateBuffer(resources, createInfo,
        vulkanBufferUsageFromUsage(createInfo.Description.Usage), flags);

    if (!createInfo.InitialData.empty())
    {
        if (enumHasAny(createInfo.Description.Usage, BufferUsage::Mappable | BufferUsage::MappableRandomAccess))
        {
            SetBufferData(resources, buffer, createInfo.InitialData, 0);
        }
        else
        {
            const Buffer stagingBuffer = CreateStagingBuffer(resources, createInfo.InitialData.size());
            SetBufferData(resources, stagingBuffer, createInfo.InitialData, 0);
            Device::ImmediateSubmit([&](RenderCommandList& cmdList)
            {
                cmdList.CopyBuffer({
                    .Source = stagingBuffer,
                    .Destination = buffer,
                    .SizeBytes = createInfo.InitialData.size()
                });
            });
            Destroy(resources, stagingBuffer);
        }
    }

    deletionQueue.Enqueue(buffer);

    return buffer;
}

void DeviceInternal::Destroy(const auto& resources, Buffer buffer)
{
    const BufferResource& resource = resources[buffer];
    vmaDestroyBuffer(Allocator(), resource.Buffer, resource.Allocation);
    resources.Remove(buffer);
}

Buffer DeviceInternal::CreateStagingBuffer(const auto& resources, u64 sizeBytes)
{
    return CreateBuffer(resources, {
            .Description = {
                .SizeBytes = sizeBytes,
                .Usage = BufferUsage::Staging | BufferUsage::Mappable,
            },
            .PersistentMapping = true
        },
        Device::DummyDeletionQueue());
}

void DeviceInternal::ResizeBuffer(const auto& resources, Buffer buffer, u64 newSize, CommandBuffer cmd, bool copyData)
{
    const BufferResource& resource = resources[buffer];
    const BufferDescription& description = resource.Description;
    const u64 oldSize = description.SizeBytes;
    if (description.SizeBytes == newSize)
        return;

    const Buffer newBuffer = CreateBuffer(resources, {
            .Description = {
                .SizeBytes = newSize,
                .Usage = description.Usage,
            },
            .PersistentMapping = resource.HostAddress != nullptr
        },
        *g_State.FrameDeletionQueue);

    /* seems very questionable
     * after this line new Buffer will inherit lifetime of old buffer,
     * and old buffer will be deleted in frame queue
     */
    std::swap(resources[buffer], resources[newBuffer]);

    /* the source and destination are intentionally swapped */
    if (copyData)
        CompileCommand(resources, cmd, {
            .Source = newBuffer,
            .Destination = buffer,
            .SizeBytes = std::min(oldSize, newSize)
        });
}

void* DeviceInternal::MapBuffer(const auto& resources, Buffer buffer)
{
    const BufferResource& resource = resources[buffer];
    void* mappedData;
    vmaMapMemory(Allocator(), resource.Allocation, &mappedData);
    return mappedData;
}

void DeviceInternal::UnmapBuffer(const auto& resources, Buffer buffer)
{
    const BufferResource& resource = resources[buffer];
    vmaUnmapMemory(Allocator(), resource.Allocation);
}

void DeviceInternal::SetBufferData(const auto& resources, Buffer buffer, Span<const std::byte> data, u64 offsetBytes)
{
    const BufferResource& resource = resources[buffer];
    vmaCopyMemoryToAllocation(Allocator(), data.data(), resource.Allocation, offsetBytes, data.size());
}

void DeviceInternal::SetBufferData(const auto&, void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    mappedAddress = (void*)((u8*)mappedAddress + offsetBytes);
    std::memcpy(mappedAddress, data.data(), data.size());
}

void* DeviceInternal::GetBufferMappedAddress(const auto& resources, Buffer buffer)
{
    return resources[buffer].HostAddress;
}

usize DeviceInternal::GetBufferSizeBytes(const auto& resources, Buffer buffer)
{
    return resources[buffer].Description.SizeBytes;
}

const BufferDescription& DeviceInternal::GetBufferDescription(const auto& resources, Buffer buffer)
{
    return resources[buffer].Description;
}

// todo: just store it like host address
u64 DeviceInternal::GetDeviceAddress(const auto& resources, Buffer buffer)
{
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = resources[buffer].Buffer;

    return vkGetBufferDeviceAddress(g_State.Device, &deviceAddressInfo);
}

Buffer DeviceInternal::AllocateBuffer(const auto& resources, const BufferCreateInfo& createInfo,
    VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = createInfo.Description.SizeBytes;
    bufferCreateInfo.usage = usage;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = allocationFlags;

    BufferResource bufferResource = {};
    deviceCheck(vmaCreateBuffer(Allocator(), &bufferCreateInfo, &allocationCreateInfo,
            &bufferResource.Buffer, &bufferResource.Allocation, nullptr),
        "Failed to create a buffer");

    bufferResource.Description.SizeBytes = createInfo.Description.SizeBytes;
    bufferResource.Description.Usage = createInfo.Description.Usage;
    if (createInfo.PersistentMapping)
        bufferResource.HostAddress = bufferResource.Allocation->GetMappedData();

    return resources.Add(bufferResource);
}

BufferArena DeviceInternal::CreateBufferArena(const auto& resources, BufferArenaCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    VmaVirtualBlockCreateInfo virtualBlockCreateInfo = {};
    virtualBlockCreateInfo.size = createInfo.VirtualSizeBytes;

    BufferArenaResource bufferArenaResource = {};
    deviceCheck(vmaCreateVirtualBlock(&virtualBlockCreateInfo, &bufferArenaResource.VirtualBlock),
        "Failed to create buffer arena");
    bufferArenaResource.Buffer = createInfo.Buffer;
    bufferArenaResource.VirtualSizeBytes = createInfo.VirtualSizeBytes;

    const BufferArena arena = resources.Add(bufferArenaResource);
    deletionQueue.Enqueue(arena);

    return arena;
}

void DeviceInternal::Destroy(const auto& resources, BufferArena arena)
{
    BufferArenaResource& bufferArenaResource = resources[arena];

    vmaClearVirtualBlock(bufferArenaResource.VirtualBlock);
    vmaDestroyVirtualBlock(bufferArenaResource.VirtualBlock);

    resources.Remove(arena);
}

void DeviceInternal::ResizeBufferArenaPhysical(const auto& resources, BufferArena arena, u64 newSize, CommandBuffer cmd,
    bool copyData)
{
    const BufferArenaResource& arenaResource = resources[arena];
    const BufferResource& bufferResource = resources[arenaResource.Buffer];
    const u64 oldSize = bufferResource.Description.SizeBytes;
    if (oldSize == newSize)
        return;

    ResizeBuffer(resources, arenaResource.Buffer, newSize, cmd, copyData);
}

Buffer DeviceInternal::GetBufferArenaUnderlyingBuffer(const auto& resources, BufferArena arena)
{
    return resources[arena].Buffer;
}

u64 DeviceInternal::GetBufferArenaSizeBytesPhysical(const auto& resources, BufferArena arena)
{
    return GetBufferSizeBytes(resources, GetBufferArenaUnderlyingBuffer(resources, arena));
}

BufferSuballocationResult DeviceInternal::BufferArenaSuballocate(const auto& resources, BufferArena arena,
    u64 sizeBytes, u32 alignment)
{
    VmaVirtualAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.size = sizeBytes + alignment;
    allocationCreateInfo.alignment = 0;
    // todo: is this ok flag to use?
    allocationCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    BufferArenaResource& bufferArenaResource = resources[arena];
    VmaVirtualAllocation allocation;
    const VkResult allocateResult = vmaVirtualAllocate(bufferArenaResource.VirtualBlock,
        &allocationCreateInfo, &allocation, nullptr);
    if (allocateResult != VK_SUCCESS)
        return std::unexpected(BufferSuballocationError::OutOfVirtualMemory);

    VmaVirtualAllocationInfo allocationInfo = {};
    vmaGetVirtualAllocationInfo(bufferArenaResource.VirtualBlock, allocation, &allocationInfo);

    if (allocationInfo.offset + allocationInfo.size > GetBufferSizeBytes(resources, bufferArenaResource.Buffer))
    {
        vmaVirtualFree(bufferArenaResource.VirtualBlock, allocation);
        return std::unexpected(BufferSuballocationError::OutOfPhysicalMemory);
    }

    if (alignment != 0)
        allocationInfo.offset = lux::mem::alignAddress(allocationInfo.offset, (u16)alignment);

    return BufferSuballocation{
        .Buffer = bufferArenaResource.Buffer,
        .Description = {
            .SizeBytes = allocationInfo.size,
            .Offset = allocationInfo.offset
        },
        .Handle = (u64)allocation
    };
}

void DeviceInternal::BufferArenaFree(const auto& resources, BufferArena arena, BufferSuballocationHandle suballocation)
{
    vmaVirtualFree(resources[arena].VirtualBlock, (VmaVirtualAllocation)suballocation);
}

Image DeviceInternal::CreateImage(const auto& resources, ImageCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    Image image = {};

    if (std::holds_alternative<const lux::assetlib::ImageAsset*>(createInfo.DataSource))
        image = CreateImageFromAssetFile(resources, createInfo,
            std::get<const lux::assetlib::ImageAsset*>(createInfo.DataSource));
    else if (std::holds_alternative<Span<const std::byte>>(createInfo.DataSource))
        image = CreateImageFromPixels(resources, createInfo, std::get<Span<const std::byte>>(createInfo.DataSource));

    deletionQueue.Enqueue(image);

    return image;
}

Image DeviceInternal::CreateImageFromAssetFile(const auto& resources, ImageCreateInfo& createInfo,
    const lux::assetlib::ImageAsset* asset)
{
    Image image = {};
    Buffer imageBuffer = {};

    ImmediateSubmit(resources, [&](RenderCommandList& cmdList)
    {
        u64 totalSizeBytes = 0;
        for (auto& mip : asset->Header.MipmapSizes)
            for (const u64 layerSize : mip)
                totalSizeBytes += layerSize;

        imageBuffer = CreateBuffer(resources, {
            .Description = {
                .SizeBytes = totalSizeBytes,
                .Usage = BufferUsage::Source | BufferUsage::StagingRandomAccess,
            },
            .PersistentMapping = true
        }, Device::DummyDeletionQueue());
        const BufferResource& imageBufferResource = resources[imageBuffer];

        image = AllocateImage(resources, createInfo);
        CreateViews(resources, ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        ImageSubresource imageSubresource = {
            .Image = image,
            .Description = {.Mipmaps = (i8)asset->Header.Mipmaps, .Layers = (i8)asset->Header.Layers}
        };

        ::DeletionQueue& deletionQueue = *g_State.FrameDeletionQueue;

        CompileCommand(resources, cmdList.m_Cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = ImageLayout::Undefined,
                    .NewLayout = ImageLayout::Destination
                }
            }, deletionQueue)
        });

        i8 mipsToCopy = createInfo.CalculateMipmaps ? (i8)1 : (i8)asset->Header.Mipmaps;
        u64 mipOffset = 0;
        u64 offset = 0;
        for (i8 mip = 0; mip < mipsToCopy; mip++)
        {
            u64 mipSize = 0;
            mipOffset = offset;
            for (i8 layer = 0; layer < (i8)asset->Header.Layers; layer++)
            {
                const u64 size = asset->Header.MipmapSizes[mip][layer];
                std::memcpy(
                    (std::byte*)imageBufferResource.HostAddress + offset,
                    asset->MipmapsImageData[mip][layer].data(),
                    size
                );
                offset += size;
                mipSize += size;
            }

            CompileCommand(resources, cmdList.m_Cmd, {
                .Buffer = imageBuffer,
                .Image = image,
                .SizeBytes = mipSize,
                .BufferOffset = mipOffset,
                .ImageSubresource = {
                    .MipmapBase = mip,
                    .Mipmaps = 1,
                    .Layers = createInfo.Description.GetLayers()
                }
            });
        }

        ImageLayout currentLayout = ImageLayout::Destination;
        if (createInfo.CalculateMipmaps)
        {
            CompileCommand(resources, cmdList.m_Cmd, MipmapImageCommand{
                .Image = image,
                .Layout = currentLayout
            });
            currentLayout = ImageLayout::Source;
            imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
        }

        CompileCommand(resources, cmdList.m_Cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = currentLayout,
                    .NewLayout = ImageLayout::Readonly
                }
            }, deletionQueue)
        });
    });

    Destroy(resources, imageBuffer);

    return image;
}

Image DeviceInternal::CreateImageFromPixels(const auto& resources, ImageCreateInfo& createInfo,
    Span<const std::byte> pixels)
{
    if (pixels.empty())
    {
        Image image = AllocateImage(resources, createInfo);
        CreateViews(resources, ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        return image;
    }

    const Buffer imageBuffer = CreateBuffer(resources, {
            .Description = {
                .SizeBytes = pixels.size(),
                .Usage = BufferUsage::Source | BufferUsage::Staging,
            },
            .InitialData = pixels
        },
        Device::DummyDeletionQueue());

    const Image image = CreateImageFromBuffer(resources, createInfo, imageBuffer);
    Destroy(resources, imageBuffer);

    return image;
}

Image DeviceInternal::CreateImageFromBuffer(const auto& resources, ImageCreateInfo& createInfo, Buffer buffer)
{
    Image image = {};

    ImmediateSubmit(resources, [&](RenderCommandList& cmdList)
    {
        image = AllocateImage(resources, createInfo);
        CreateViews(resources, ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        ImageSubresource imageSubresource = {.Image = image, .Description = {.Mipmaps = 1, .Layers = 1}};

        ::DeletionQueue& deletionQueue = *g_State.FrameDeletionQueue;

        CompileCommand(resources, cmdList.m_Cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = ImageLayout::Undefined,
                    .NewLayout = ImageLayout::Destination
                }
            }, deletionQueue)
        });

        CompileCommand(resources, cmdList.m_Cmd, CopyBufferToImageCommand{
            .Buffer = buffer,
            .Image = image,
            .ImageSubresource = {
                .Mipmaps = 1,
                .Layers = createInfo.Description.GetLayers()
            }
        });

        ImageLayout currentLayout = ImageLayout::Destination;
        if (createInfo.CalculateMipmaps)
        {
            CompileCommand(resources, cmdList.m_Cmd, MipmapImageCommand{
                .Image = image,
                .Layout = currentLayout
            });
            currentLayout = ImageLayout::Source;
            imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
        }

        CompileCommand(resources, cmdList.m_Cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = currentLayout,
                    .NewLayout = ImageLayout::Readonly
                }
            }, deletionQueue)
        });
    });

    return image;
}

Image DeviceInternal::CreateEmptyImage(const auto& resources, ImageCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    const Image image = AllocateImage(resources, createInfo);
    CreateViews(resources, ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

    deletionQueue.Enqueue(image);

    return image;
}

void DeviceInternal::PreprocessCreateInfo(ImageCreateInfo& createInfo)
{
    if (std::holds_alternative<const lux::assetlib::ImageAsset*>(createInfo.DataSource))
        createInfo.Description.Usage |= ImageUsage::Destination;

    if (createInfo.Description.Mipmaps > 1)
        createInfo.Description.Usage |= ImageUsage::Destination | ImageUsage::Source;

    if (createInfo.Description.Kind == ImageKind::ImageCubemap)
        createInfo.Description.LayersDepth = 6;

    if (enumHasAny(createInfo.Description.Usage, ImageUsage::Readback))
        createInfo.Description.Usage |= ImageUsage::Source;
}

Image DeviceInternal::AllocateImage(const auto& resources, ImageCreateInfo& createInfo)
{
    PreprocessCreateInfo(createInfo);

    u32 depth = createInfo.Description.GetDepth();
    u32 layers = (u32)(u8)createInfo.Description.GetLayers();

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = vulkanFormatFromFormat(createInfo.Description.Format);
    imageCreateInfo.usage = vulkanImageUsageFromImageUsage(createInfo.Description.Usage);
    imageCreateInfo.extent = {
        .width = createInfo.Description.Width,
        .height = createInfo.Description.Height,
        .depth = depth
    };
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.imageType = vulkanImageTypeFromImageKind(createInfo.Description.Kind);
    imageCreateInfo.mipLevels = (u32)(u8)createInfo.Description.Mipmaps;
    imageCreateInfo.arrayLayers = layers;
    imageCreateInfo.flags = vulkanImageFlagsFromImageKind(createInfo.Description.Kind);

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = enumHasAny(createInfo.Description.Usage,
            ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil) ?
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT :
        0;

    ImageResource imageResource = {};
    deviceCheck(vmaCreateImage(Allocator(), &imageCreateInfo, &allocationInfo,
            &imageResource.Image, &imageResource.Allocation, nullptr),
        "Failed to create image");
    imageResource.Description = createInfo.Description;

    return resources.Add(imageResource);
}

void DeviceInternal::Destroy(const auto& resources, Image image)
{
    const ImageResource& imageResource = resources[image];
    if (imageResource.Views.ViewList == &imageResource.Views.ViewType.View)
    {
        vkDestroyImageView(g_State.Device, imageResource.Views.ViewType.View, nullptr);
    }
    else
    {
        for (u32 viewIndex = 0; viewIndex < imageResource.Views.ViewType.ViewCount; viewIndex++)
            vkDestroyImageView(g_State.Device, imageResource.Views.ViewList[viewIndex], nullptr);
        delete[] imageResource.Views.ViewList;
    }
    vmaDestroyImage(Allocator(), imageResource.Image, imageResource.Allocation);
    resources.Remove(image);
}

void DeviceInternal::CreateViews(const auto& resources, const ImageSubresource& image,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    ImageResource& resource = resources[image.Image];
    VkFormat viewFormat = vulkanFormatFromFormat(resource.Description.Format);
    if (additionalViews.empty())
    {
        resource.Views.ViewType.View = CreateVulkanImageView(resources, image, viewFormat);
        resource.Views.ViewList = &resource.Views.ViewType.View;
        return;
    }

    resource.Views.ViewType.ViewCount = 1 + (u32)resource.Description.AdditionalViews.size();
    resource.Views.ViewList = new VkImageView[resource.Views.ViewType.ViewCount];
    resource.Views.ViewList[0] = CreateVulkanImageView(resources, image, viewFormat);
    for (u32 viewIndex = 0; viewIndex < additionalViews.size(); viewIndex++)
        resource.Views.ViewList[viewIndex + 1] = CreateVulkanImageView(
            resources, ImageSubresource{.Image = image.Image, .Description = additionalViews[viewIndex]}, viewFormat);
}

VkImageView DeviceInternal::CreateVulkanImageView(const auto& resources, const ImageSubresource& image, VkFormat format)
{
    const ImageResource& imageResource = resources[image.Image];
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = resources[image.Image].Image;
    createInfo.format = format;
    createInfo.viewType = vulkanImageViewTypeFromImageAndViewKind(imageResource.Description.Kind,
        image.Description.ImageViewKind);
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = vulkanImageAspectFromImageUsage(imageResource.Description.Usage);
    createInfo.subresourceRange.baseMipLevel = (u32)(i32)image.Description.MipmapBase;
    createInfo.subresourceRange.levelCount = (u32)(i32)image.Description.Mipmaps;
    createInfo.subresourceRange.baseArrayLayer = (u32)(i32)image.Description.LayerBase;
    createInfo.subresourceRange.layerCount = (u32)(i32)image.Description.Layers;

    VkImageView imageView;

    deviceCheck(vkCreateImageView(g_State.Device, &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

Span<const ImageSubresourceDescription> DeviceInternal::GetAdditionalImageViews(const auto& resources, Image image)
{
    return resources[image].Description.AdditionalViews;
}

ImageViewHandle DeviceInternal::GetImageViewHandle(const auto& resources, Image image,
    ImageSubresourceDescription subresourceDescription)
{
    if (subresourceDescription == ImageSubresourceDescription{})
        return 0;

    const ImageDescription& description = resources[image].Description;
    auto it = std::ranges::find(description.AdditionalViews, subresourceDescription);

    if (it != description.AdditionalViews.end())
        return ImageViewHandle{u32(it - description.AdditionalViews.begin()) + 1};

    LUX_LOG_ERROR("Image does not have such view subresource, returning default view");
    return ImageViewHandle{};
}

const ImageDescription& DeviceInternal::GetImageDescription(const auto& resources, Image image)
{
    return resources[image].Description;
}

ImTextureID DeviceInternal::CreateImGuiImage(const auto& resources, const ImageSubresource& texture, Sampler sampler,
    ImageLayout layout)
{
    const ImageViewHandle viewHandle = GetImageViewHandle(resources, texture.Image, texture.Description);
    const VkDescriptorSet imageDescriptorSet = ImGui_ImplVulkan_AddTexture(resources[sampler].Sampler,
        resources[texture.Image].Views.ViewList[viewHandle.m_Index],
        vulkanImageLayoutFromImageLayout(layout));

    return ImTextureID{imageDescriptorSet};
}

void DeviceInternal::DestroyImGuiImage(ImTextureID image)
{
    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)image);
}

Sampler DeviceInternal::CreateSampler(const auto& resources, SamplerCreateInfo&& createInfo)
{
    const SamplerCache::CacheKey key = SamplerCache::CreateCacheKey(createInfo);
    Sampler cached = SamplerCache::Find(key);
    if (cached.HasValue())
        return cached;

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = vulkanFilterFromImageFilter(createInfo.MagnificationFilter);
    samplerCreateInfo.minFilter = vulkanFilterFromImageFilter(createInfo.MinificationFilter);
    samplerCreateInfo.mipmapMode = vulkanMipmapModeFromSamplerFilter(samplerCreateInfo.minFilter);
    samplerCreateInfo.addressModeU = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.WrapMode);
    samplerCreateInfo.addressModeV = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.WrapMode);
    samplerCreateInfo.addressModeW = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.WrapMode);
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = createInfo.MaxLod;
    samplerCreateInfo.maxAnisotropy = Device::GetAnisotropyLevel();
    samplerCreateInfo.anisotropyEnable = (u32)createInfo.WithAnisotropy;
    samplerCreateInfo.mipLodBias = createInfo.LodBias;
    samplerCreateInfo.compareEnable =
        isVulkanSamplerCompareOpEnabledFromSamplerDepthCompareMode(createInfo.DepthCompareMode);
    samplerCreateInfo.compareOp = vulkanSamplerCompareOpFromSamplerDepthCompareMode(createInfo.DepthCompareMode);
    samplerCreateInfo.borderColor = vulkanBorderColorFromBorderColor(createInfo.BorderColor);

    VkSamplerReductionModeCreateInfo reductionModeCreateInfo = {};
    if (createInfo.ReductionMode.has_value())
    {
        reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        reductionModeCreateInfo.reductionMode = vulkanSamplerReductionModeFromSamplerReductionMode(
            *createInfo.ReductionMode);
        samplerCreateInfo.pNext = &reductionModeCreateInfo;
    }

    SamplerResource samplerResource = {};
    deviceCheck(vkCreateSampler(g_State.Device, &samplerCreateInfo, nullptr, &samplerResource.Sampler),
        "Failed to create depth pyramid sampler");

    const Sampler sampler = resources.Add(samplerResource);
    Device::DeletionQueue().Enqueue(sampler);

    SamplerCache::Emplace(key, sampler);

    return sampler;
}

void DeviceInternal::Destroy(const auto& resources, Sampler sampler)
{
    vkDestroySampler(g_State.Device, resources[sampler].Sampler, nullptr);
    resources.Remove(sampler);
}

RenderingAttachment DeviceInternal::CreateRenderingAttachment(const auto& resources,
    RenderingAttachmentCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    RenderingAttachmentResource renderingAttachmentResource = {};

    renderingAttachmentResource.AttachmentInfo = {};
    renderingAttachmentResource.AttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderingAttachmentResource.AttachmentInfo.clearValue = VkClearValue{
        .color = {
            .float32 = {
                createInfo.Description.Clear.Color.F.r,
                createInfo.Description.Clear.Color.F.g,
                createInfo.Description.Clear.Color.F.b,
                createInfo.Description.Clear.Color.F.a
            }
        }
    };
    renderingAttachmentResource.AttachmentInfo.imageLayout = vulkanImageLayoutFromImageLayout(
        createInfo.Layout);
    renderingAttachmentResource.AttachmentInfo.imageView = resources[createInfo.Image].Views.ViewList[
        GetImageViewHandle(resources, createInfo.Image, createInfo.Description.Subresource).m_Index];
    renderingAttachmentResource.AttachmentInfo.loadOp = vulkanAttachmentLoadFromAttachmentLoad(
        createInfo.Description.OnLoad);
    renderingAttachmentResource.AttachmentInfo.storeOp = vulkanAttachmentStoreFromAttachmentStore(
        createInfo.Description.OnStore);
    renderingAttachmentResource.AttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;

    RenderingAttachment renderingAttachment = resources.Add(renderingAttachmentResource);
    deletionQueue.Enqueue(renderingAttachment);

    return renderingAttachment;
}

void DeviceInternal::Destroy(const auto& resources, RenderingAttachment renderingAttachment)
{
    resources.Remove(renderingAttachment);
}

RenderingInfo DeviceInternal::CreateRenderingInfo(const auto& resources, RenderingInfoCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    RenderingInfoResource renderingInfoResource = {};
    renderingInfoResource.ColorAttachments.reserve(createInfo.ColorAttachments.size());

    for (auto& attachment : createInfo.ColorAttachments)
        renderingInfoResource.ColorAttachments.push_back(resources[attachment].AttachmentInfo);
    if (createInfo.DepthAttachment.has_value())
        renderingInfoResource.DepthAttachment = resources[*createInfo.DepthAttachment].AttachmentInfo;

    renderingInfoResource.RenderArea = createInfo.RenderArea;
    const RenderingInfo renderingInfo = resources.Add(renderingInfoResource);
    deletionQueue.Enqueue(renderingInfo);

    return renderingInfo;
}

void DeviceInternal::Destroy(const auto& resources, RenderingInfo renderingInfo)
{
    resources.Remove(renderingInfo);
}

PipelineLayout DeviceInternal::CreatePipelineLayout(const auto& resources, PipelineLayoutCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(createInfo.PushConstants.size());
    std::vector<VkDescriptorSetLayout> descriptorsLayouts;
    descriptorsLayouts.reserve(createInfo.DescriptorsLayouts.size());
    for (auto& pushConstant : createInfo.PushConstants)
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.size = pushConstant.SizeBytes;
        pushConstantRange.offset = pushConstant.Offset;
        pushConstantRange.stageFlags = vulkanShaderStageFromShaderStage(pushConstant.StageFlags);

        pushConstantRanges.push_back(pushConstantRange);
    }
    for (auto& descriptorLayout : createInfo.DescriptorsLayouts)
        descriptorsLayouts.push_back(resources[descriptorLayout].Layout);

    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pushConstantRangeCount = (u32)pushConstantRanges.size();
    layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
    layoutCreateInfo.setLayoutCount = (u32)descriptorsLayouts.size();
    layoutCreateInfo.pSetLayouts = descriptorsLayouts.data();

    PipelineLayoutResource pipelineLayoutResource = {};
    pipelineLayoutResource.PushConstants = pushConstantRanges;
    deviceCheck(vkCreatePipelineLayout(g_State.Device, &layoutCreateInfo, nullptr,
        &pipelineLayoutResource.Layout), "Failed to create pipeline layout");

    PipelineLayout layout = resources.Add(pipelineLayoutResource);
    deletionQueue.Enqueue(layout);

    return layout;
}

void DeviceInternal::Destroy(const auto& resources, PipelineLayout pipelineLayout)
{
    vkDestroyPipelineLayout(g_State.Device, resources[pipelineLayout].Layout, nullptr);
    resources.Remove(pipelineLayout);
}

Pipeline DeviceInternal::CreatePipeline(const auto& resources, PipelineCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    VkPipelineLayout layout = resources[createInfo.PipelineLayout].Layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    shaders.reserve(createInfo.Shaders.size());
    for (auto&& [i, shader] : std::views::enumerate(createInfo.Shaders))
    {
        auto& module = resources[shader];

        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.stage = vulkanStageBitFromShaderStage(createInfo.ShaderStages[i]);
        shaderStageCreateInfo.module = module.Module;
        shaderStageCreateInfo.pName = createInfo.ShaderEntryPoints[i].data();

        shaders.push_back(shaderStageCreateInfo);
    }

    std::vector<std::vector<VkSpecializationMapEntry>> shaderSpecializationEntries(createInfo.Shaders.size());
    std::vector<VkSpecializationInfo> shaderSpecializationInfos;
    shaderSpecializationInfos.reserve(createInfo.Shaders.size());
    for (u32 shaderIndex = 0; shaderIndex < createInfo.Shaders.size(); shaderIndex++)
    {
        auto& shader = shaders[shaderIndex];
        VkSpecializationInfo shaderSpecializationInfo = {};
        for (const auto& specialization : createInfo.Specialization.Descriptions)
            if (enumHasAny(
                shader.stage, (VkShaderStageFlagBits)vulkanShaderStageFromShaderStage(specialization.ShaderStages)))
                shaderSpecializationEntries[shaderIndex].push_back({
                    .constantID = specialization.Id,
                    .offset = specialization.Offset,
                    .size = specialization.SizeBytes
                });

        shaderSpecializationInfo.dataSize = createInfo.Specialization.Data.size();
        shaderSpecializationInfo.pData = createInfo.Specialization.Data.data();
        shaderSpecializationInfo.mapEntryCount = (u32)shaderSpecializationEntries[shaderIndex].size();
        shaderSpecializationInfo.pMapEntries = shaderSpecializationEntries[shaderIndex].data();

        shaderSpecializationInfos.push_back(shaderSpecializationInfo);
        shader.pSpecializationInfo = &shaderSpecializationInfos.back();
    }

    Pipeline pipeline = {};
    if (createInfo.IsComputePipeline)
    {
        VkComputePipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stage = shaders.front();
        pipelineCreateInfo.layout = layout;

#ifdef DESCRIPTOR_BUFFER
        pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif

        PipelineResource pipelineResource = {};
        deviceCheck(vkCreateComputePipelines(g_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create compute pipeline");
        pipeline = resources.Add(pipelineResource);
    }
    else
    {
        ASSERT(
            !createInfo.ColorFormats.empty() ||
            createInfo.DepthFormat != Format::Undefined, "No rendering details provided")

        std::vector<VkDynamicState> dynamicStates = vulkanDynamicStatesFromDynamicStates(createInfo.DynamicStates);

        VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = (u32)dynamicStates.size();
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
        inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyState.primitiveRestartEnable = VK_FALSE;
        inputAssemblyState.topology = vulkanTopologyFromPrimitiveKind(createInfo.PrimitiveKind);

        VkPipelineVertexInputStateCreateInfo vertexInputState = {};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
        bindings.reserve(createInfo.VertexDescription.Bindings.size());
        attributes.reserve(createInfo.VertexDescription.Attributes.size());
        for (auto& binding : createInfo.VertexDescription.Bindings)
            bindings.push_back({
                .binding = binding.Index,
                .stride = binding.StrideBytes,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            });

        for (auto& attribute : createInfo.VertexDescription.Attributes)
            attributes.push_back({
                .location = attribute.Index,
                .binding = attribute.BindingIndex,
                .format = vulkanFormatFromFormat(attribute.Format),
                .offset = attribute.OffsetBytes
            });
        vertexInputState.vertexBindingDescriptionCount = (u32)bindings.size();
        vertexInputState.pVertexBindingDescriptions = bindings.data();
        vertexInputState.vertexAttributeDescriptionCount = (u32)attributes.size();
        vertexInputState.pVertexAttributeDescriptions = attributes.data();

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.depthClampEnable = createInfo.ClampDepth ? VK_TRUE : VK_FALSE;
        rasterizationState.depthBiasEnable = enumHasAny(createInfo.DynamicStates, DynamicStates::DepthBias);
        rasterizationState.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0f;
        rasterizationState.cullMode = vulkanCullModeFromFaceCullMode(createInfo.CullMode);
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.sampleShadingEnable = VK_FALSE;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable = createInfo.DepthMode == DepthMode::None ? VK_FALSE : VK_TRUE;
        depthStencilState.depthWriteEnable =
            (createInfo.DepthMode == DepthMode::None ||
                createInfo.DepthMode == DepthMode::Read) ?
            VK_FALSE :
            VK_TRUE;
        depthStencilState.depthCompareOp = createInfo.DepthTest == DepthTest::GreaterOrEqual ?
            VK_COMPARE_OP_GREATER_OR_EQUAL :
            VK_COMPARE_OP_EQUAL;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
        colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        switch (createInfo.AlphaBlending)
        {
        case AlphaBlending::None:
            colorBlendAttachmentState.blendEnable = VK_FALSE;
            break;
        case AlphaBlending::Over:
            colorBlendAttachmentState.blendEnable = VK_TRUE;
            colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
            break;
        default:
            ASSERT(false, "Unsupported blending mode")
        }

        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.logicOpEnable = VK_FALSE;
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorBlendAttachmentState;

        std::vector<VkFormat> colorFormats;
        colorFormats.resize(createInfo.ColorFormats.size());

        for (u32 colorIndex = 0; colorIndex < createInfo.ColorFormats.size(); colorIndex++)
            colorFormats[colorIndex] = vulkanFormatFromFormat(createInfo.ColorFormats[colorIndex]);

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = (u32)colorFormats.size();
        pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats.data();
        pipelineRenderingCreateInfo.depthAttachmentFormat = vulkanFormatFromFormat(
            createInfo.DepthFormat);

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = (u32)shaders.size();
        pipelineCreateInfo.pStages = shaders.data();
        pipelineCreateInfo.pDynamicState = &dynamicStateInfo;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pVertexInputState = &vertexInputState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.layout = layout;
        pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

#ifdef DESCRIPTOR_BUFFER
        pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif

        PipelineResource pipelineResource = {};
        deviceCheck(vkCreateGraphicsPipelines(g_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create graphics pipeline");
        pipeline = resources.Add(pipelineResource);
    }
    deletionQueue.Enqueue(pipeline);

    return pipeline;
}

void DeviceInternal::Destroy(const auto& resources, Pipeline pipeline)
{
    vkDestroyPipeline(g_State.Device, resources[pipeline].Pipeline, nullptr);
    resources.Remove(pipeline);
}

ShaderModule DeviceInternal::CreateShaderModule(const auto& resources, ShaderModuleCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = createInfo.Source.size();
    moduleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.Source.data());

    ShaderModuleResource shaderModuleResource = {};
    deviceCheck(vkCreateShaderModule(g_State.Device, &moduleCreateInfo, nullptr, &shaderModuleResource.Module),
        "Failed to create shader module");

    ShaderModule module = resources.Add(shaderModuleResource);
    deletionQueue.Enqueue(module);

    return module;
}

void DeviceInternal::Destroy(const auto& resources, ShaderModule shaderModule)
{
    vkDestroyShaderModule(g_State.Device, resources[shaderModule].Module, nullptr);
    resources.Remove(shaderModule);
}

DescriptorsLayout DeviceInternal::CreateDescriptorsLayout(const auto& resources,
    DescriptorsLayoutCreateInfo&& createInfo)
{
    const DescriptorLayoutCache::CacheKey key = DescriptorLayoutCache::CreateCacheKey(createInfo);
    DescriptorsLayout cached = DescriptorLayoutCache::Find(key);
    if (cached.HasValue())
        return cached;

    std::vector<VkDescriptorBindingFlags> bindingFlags;
    bindingFlags.reserve(createInfo.Bindings.size());

#ifdef DESCRIPTOR_BUFFER
    for (auto binding : createInfo.Bindings)
        bindingFlags.push_back(vulkanDescriptorBindingFlagsFromDescriptorFlags(binding.Flags));
#else
    for (auto binding : createInfo.Bindings)
    {
        DescriptorFlags flags = binding.Flags;
        if (enumHasAny(flags, DescriptorFlags::VariableCount))
            flags |= DescriptorFlags::UpdateAfterBind |
                DescriptorFlags::UpdateUnusedPending;
        bindingFlags.push_back(vulkanDescriptorBindingFlagsFromDescriptorFlags(
            flags | DescriptorFlags::PartiallyBound));
    }
#endif

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(createInfo.Bindings.size());
    DescriptorLayoutFlags layoutFlags = createInfo.Flags;

    bool layoutHasImmutableSamplers = false;
    for (auto& binding : createInfo.Bindings)
    {
        bindings.push_back({
            .binding = binding.Binding,
            .descriptorType = vulkanDescriptorTypeFromDescriptorType(binding.Type),
            .descriptorCount = binding.Count,
            .stageFlags = vulkanShaderStageFromShaderStage(binding.Shaders),
            .pImmutableSamplers = nullptr
        });

        layoutHasImmutableSamplers |= binding.ImmutableSampler.HasValue();
        if (binding.ImmutableSampler.HasValue())
            bindings.back().pImmutableSamplers = &resources[binding.ImmutableSampler].Sampler;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.bindingCount = (u32)bindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

#ifdef DESCRIPTOR_BUFFER
    layoutFlags |= DescriptorLayoutFlags::DescriptorBuffer;
    if (layoutHasImmutableSamplers)
        layoutFlags |= DescriptorLayoutFlags::EmbeddedImmutableSamplers;
    layoutFlags &= ~DescriptorLayoutFlags::UpdateAfterBind;
#else
    (void)layoutHasImmutableSamplers;
#endif

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = (u32)bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.flags = vulkanDescriptorsLayoutFlagsFromDescriptorsLayoutFlags(layoutFlags);
    layoutCreateInfo.pNext = &bindingFlagsCreateInfo;

    DescriptorsLayoutResource descriptorSetLayoutResource = {};
    deviceCheck(vkCreateDescriptorSetLayout(g_State.Device, &layoutCreateInfo, nullptr,
        &descriptorSetLayoutResource.Layout), "Failed to create descriptor set layout");

    DescriptorsLayout layout = resources.Add(descriptorSetLayoutResource);
    Device::DeletionQueue().Enqueue(layout);

    DescriptorLayoutCache::Emplace(key, layout);

    return layout;
}

DescriptorsLayout DeviceInternal::GetEmptyDescriptorsLayout(const auto& resources)
{
#ifdef DESCRIPTOR_BUFFER
    static const DescriptorsLayout EMPTY_LAYOUT =
        CreateDescriptorsLayout(resources, {.Flags = DescriptorLayoutFlags::DescriptorBuffer});
#else
    static const DescriptorsLayout EMPTY_LAYOUT =
        CreateDescriptorsLayout(resources, {});
#endif

    return EMPTY_LAYOUT;
}

void DeviceInternal::Destroy(const auto& resources, DescriptorsLayout layout)
{
    vkDestroyDescriptorSetLayout(g_State.Device, resources[layout].Layout, nullptr);
    resources.Remove(layout);
}


#ifdef DESCRIPTOR_BUFFER
DescriptorArenaAllocator DeviceInternal::CreateDescriptorArenaAllocator(const auto& resources,
    DescriptorArenaAllocatorCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    ASSERT(!createInfo.UsedTypes.empty(), "At least one descriptor type is necessary")
    ASSERT(createInfo.Residence == DescriptorAllocatorResidence::CPU, "GPU residence is not supported")

    VkBufferUsageFlags descriptorBufferUsage = 0;
    for (auto type : createInfo.UsedTypes)
    {
        if (type == DescriptorType::Sampler)
            descriptorBufferUsage |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
        else
            descriptorBufferUsage |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
    }

    u32 maxDescriptorSize = 0;
    for (auto type : createInfo.UsedTypes)
        maxDescriptorSize = std::max(maxDescriptorSize, DeviceInternal::GetDescriptorSizeBytes(type));

    const u64 arenaSizeBytes = (u64)maxDescriptorSize * createInfo.DescriptorCount;

    VkBufferUsageFlags usageFlags = descriptorBufferUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateFlags allocationFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if (createInfo.Residence == DescriptorAllocatorResidence::GPU)
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    else
        allocationFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    DescriptorArenaAllocatorResource allocatorResource = {};
    allocatorResource.DescriptorSet = createInfo.DescriptorSet;
    allocatorResource.DescriptorBufferUsage = descriptorBufferUsage;
    allocatorResource.Residence = createInfo.Residence;
    allocatorResource.SizeBytes = arenaSizeBytes;
    allocatorResource.Descriptors.reserve(createInfo.DescriptorCount);
    const BufferCreateInfo arenaCreateInfo = {.Description = {.SizeBytes = arenaSizeBytes}, .PersistentMapping = true};
    allocatorResource.Arena = AllocateBuffer(resources, arenaCreateInfo, usageFlags, allocationFlags);
    allocatorResource.DeviceAddress = GetDeviceAddress(resources, allocatorResource.Arena);
    allocatorResource.MappedAddress = GetBufferMappedAddress(resources, allocatorResource.Arena);

    DescriptorArenaAllocator allocator = resources.Add(allocatorResource);
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void DeviceInternal::Destroy(const auto& resources, DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator(resources, allocator);
    Destroy(resources, resources[allocator].Arena);
    resources.Remove(allocator);
}

std::optional<Descriptors> DeviceInternal::AllocateDescriptors(const auto& resources,
    DescriptorArenaAllocator allocator, DescriptorsLayout layout, DescriptorAllocatorAllocationBindings&& bindings)
{
    DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    auto& descriptorBufferProps = g_State.GPUDescriptorBufferProperties;

    /* if we have bindless binding, we have to calculate layout size as a sum of bindings sizes */
    u64 layoutSizeBytes = 0;
    if (bindings.BindlessCount == 0)
    {
        vkGetDescriptorSetLayoutSizeEXT(g_State.Device, resources[layout].Layout, &layoutSizeBytes);
    }
    else
    {
        for (u32 bindingIndex = 0; bindingIndex < bindings.Bindings.size(); bindingIndex++)
        {
            auto& binding = bindings.Bindings[bindingIndex];
            bool isBindless = enumHasAny(binding.Flags, DescriptorFlags::VariableCount);
            ASSERT(
                (bindingIndex == (u32)bindings.Bindings.size() - 1 && isBindless) ||
                (bindingIndex != (u32)bindings.Bindings.size() - 1 && !isBindless),
                "Only one binding can be declared as 'bindless' for any particular set, and it has to be the last one")

            layoutSizeBytes += isBindless ?
                bindings.BindlessCount * GetDescriptorSizeBytes(binding.Type) :
                GetDescriptorSizeBytes(binding.Type);
        }
    }

    layoutSizeBytes =
        lux::mem::alignAddressPow2(layoutSizeBytes, (u16)descriptorBufferProps.descriptorBufferOffsetAlignment);
    if (layoutSizeBytes + allocatorResource.CurrentOffset > allocatorResource.SizeBytes)
        return {};

    std::vector<u64> bindingOffsets(bindings.Bindings.size());
    for (u32 offsetIndex = 0; offsetIndex < bindingOffsets.size(); offsetIndex++)
    {
        auto& binding = bindings.Bindings[offsetIndex];
        vkGetDescriptorSetLayoutBindingOffsetEXT(g_State.Device, resources[layout].Layout, binding.Binding,
            &bindingOffsets[offsetIndex]);
        bindingOffsets[offsetIndex] += allocatorResource.CurrentOffset;
    }

    DescriptorsResource descriptorsResource = {};
    descriptorsResource.Offsets = bindingOffsets;
    descriptorsResource.SizeBytes = layoutSizeBytes;
    descriptorsResource.Allocator = allocator;

    Descriptors descriptors = resources.Add(descriptorsResource);
    allocatorResource.Descriptors.push_back(descriptors);

    allocatorResource.CurrentOffset += layoutSizeBytes;

    return descriptors;
}

void DeviceInternal::ResetDescriptorArenaAllocator(const auto& resources, DescriptorArenaAllocator allocator)
{
    DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    for (Descriptors descriptors : allocatorResource.Descriptors)
        resources.Remove(descriptors);
    allocatorResource.Descriptors.clear();

    allocatorResource.CurrentOffset = 0;
}
void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type != DescriptorType::TexelStorage && type != DescriptorType::TexelUniform,
        "Texel buffers require format information")
    ASSERT(type != DescriptorType::StorageBufferDynamic && type != DescriptorType::UniformBufferDynamic,
        "Dynamic buffers are not supported when using descriptor buffer")

    const BufferResource& bufferResource = resources[buffer.Buffer];
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = bufferResource.Buffer;
    u64 deviceAddress = vkGetBufferDeviceAddress(g_State.Device, &deviceAddressInfo);

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.address = deviceAddress + buffer.Description.Offset;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
    ASSERT(
        buffer.Description.SizeBytes <= bufferResource.Description.SizeBytes ||
        buffer.Description.SizeBytes == BufferSubresourceDescription::WHOLE_SIZE,
        "Buffer subresource size is too large")
    descriptorAddressInfo.range = std::min(buffer.Description.SizeBytes, bufferResource.Description.SizeBytes);

    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    // using the fact that 'descriptorGetInfo.data' is union
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    WriteDescriptor(resources, descriptors, slotInfo, index, descriptorGetInfo);
}

void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type == DescriptorType::Sampler)

    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    descriptorGetInfo.data.pSampler = &resources[sampler].Sampler;
    WriteDescriptor(resources, descriptors, slotInfo, 0, descriptorGetInfo);
}

void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.imageView =
        resources[image.Image].Views.ViewList[GetImageViewHandle(resources, image.Image, image.Description).m_Index];
    descriptorImageInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);
    descriptorGetInfo.data.pSampledImage = &descriptorImageInfo;

    WriteDescriptor(resources, descriptors, slotInfo, index, descriptorGetInfo);
}
#else // DESCRIPTOR_BUFFER
DescriptorArenaAllocator DeviceInternal::CreateDescriptorArenaAllocator(const auto& resources,
    DescriptorArenaAllocatorCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    DescriptorArenaAllocatorResource descriptorAllocatorResource = {};
    descriptorAllocatorResource.MaxSetsPerPool = createInfo.DescriptorCount;
    descriptorAllocatorResource.Descriptors.reserve(createInfo.DescriptorCount);

    const DescriptorArenaAllocator allocator = resources.Add(descriptorAllocatorResource);
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void DeviceInternal::Destroy(const auto& resources, DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator(resources, allocator);

    const DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkDestroyDescriptorPool(g_State.Device, pool.Pool, nullptr);
    for (auto& pool : allocatorResource.UsedPools)
        vkDestroyDescriptorPool(g_State.Device, pool.Pool, nullptr);

    resources.Remove(allocator);
}

std::optional<Descriptors> DeviceInternal::AllocateDescriptors(const auto& resources,
    DescriptorArenaAllocator allocator, DescriptorsLayout layout, DescriptorAllocatorAllocationBindings&& bindings)
{
    const bool hasBindless = bindings.BindlessCount > 0;
    const DescriptorPoolFlags poolFlags = hasBindless ?
        DescriptorPoolFlags::UpdateAfterBind :
        DescriptorPoolFlags::None;

    DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    u32 poolIndex = GetFreePoolIndexFromAllocator(resources, allocator, poolFlags);
    VkDescriptorPool pool = allocatorResource.FreePools[poolIndex].Pool;

    VkDescriptorSetVariableDescriptorCountAllocateInfo vulkanVariableBindingCounts = {};
    vulkanVariableBindingCounts.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    vulkanVariableBindingCounts.descriptorSetCount = (u32)hasBindless;
    vulkanVariableBindingCounts.pDescriptorCounts = &bindings.BindlessCount;

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &resources[layout].Layout;
    allocateInfo.pNext = &vulkanVariableBindingCounts;

    DescriptorsResource descriptorSetResource = {};
    vkAllocateDescriptorSets(g_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet);
    descriptorSetResource.Pool = pool;

    if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
    {
        allocatorResource.UsedPools.push_back(allocatorResource.FreePools[poolIndex]);
        allocatorResource.FreePools.erase(allocatorResource.FreePools.begin() + poolIndex);

        poolIndex = GetFreePoolIndexFromAllocator(resources, allocator, poolFlags);
        pool = allocatorResource.FreePools[poolIndex].Pool;
        allocateInfo.descriptorPool = pool;
        allocateInfo.pSetLayouts = &resources[descriptorSetResource.Layout].Layout;
        vkAllocateDescriptorSets(g_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet);
        if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
            return std::nullopt;

        descriptorSetResource.Pool = pool;
    }

    const Descriptors set = resources.Add(descriptorSetResource);
    allocatorResource.Descriptors.push_back(set);

    return set;
}

void DeviceInternal::ResetDescriptorArenaAllocator(const auto& resources, DescriptorArenaAllocator allocator)
{
    DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkResetDescriptorPool(g_State.Device, pool.Pool, 0);
    for (auto pool : allocatorResource.UsedPools)
    {
        vkResetDescriptorPool(g_State.Device, pool.Pool, 0);
        allocatorResource.FreePools.push_back(pool);
    }
    allocatorResource.UsedPools.clear();
    for (Descriptors set : allocatorResource.Descriptors)
        resources.Remove(set);
    allocatorResource.Descriptors.clear();
}

void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = resources[buffer.Buffer].Buffer;
    descriptorBufferInfo.offset = buffer.Description.Offset;
    descriptorBufferInfo.range = buffer.Description.SizeBytes;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = resources[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pBufferInfo = &descriptorBufferInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}

void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = resources[sampler].Sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = resources[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}

void DeviceInternal::UpdateDescriptors(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.imageView = resources[image.Image].Views.ViewList[GetImageViewHandle(
        resources, image.Image, image.Description).m_Index];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = resources[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}
#endif // DESCRIPTOR_BUFFER

Fence DeviceInternal::CreateFence(const auto& resources, FenceCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (createInfo.IsSignaled)
        fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        fenceCreateInfo.flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    FenceResource fenceResource = {};
    deviceCheck(vkCreateFence(g_State.Device, &fenceCreateInfo, nullptr, &fenceResource.Fence),
        "Failed to create fence");

    const Fence fence = resources.Add(fenceResource);
    deletionQueue.Enqueue(fence);

    return fence;
}

void DeviceInternal::Destroy(const auto& resources, Fence fence)
{
    vkDestroyFence(g_State.Device, resources[fence].Fence, nullptr);
    resources.Remove(fence);
}

void DeviceInternal::WaitForFence(const auto& resources, Fence fence)
{
    deviceCheck(vkWaitForFences(g_State.Device, 1, &resources[fence].Fence, true, 10'000'000'000),
        "Error while waiting for fences");
}

bool DeviceInternal::CheckFence(const auto& resources, Fence fence)
{
    const VkResult result = vkGetFenceStatus(g_State.Device, resources[fence].Fence);
    return result == VK_SUCCESS;
}

void DeviceInternal::ResetFence(const auto& resources, Fence fence)
{
    deviceCheck(vkResetFences(g_State.Device, 1, &resources[fence].Fence), "Error while resetting fences");
}

Semaphore DeviceInternal::CreateSemaphore(const auto& resources, DeletionQueue& deletionQueue)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    SemaphoreResource semaphoreResource = {};
    deviceCheck(vkCreateSemaphore(g_State.Device, &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore),
        "Failed to create semaphore");

    const Semaphore semaphore = resources.Add(semaphoreResource);
    deletionQueue.Enqueue(semaphore);

    return semaphore;
}

void DeviceInternal::Destroy(const auto& resources, Semaphore semaphore)
{
    vkDestroySemaphore(g_State.Device, resources[semaphore].Semaphore, nullptr);
    resources.Remove(semaphore);
}

TimelineSemaphore DeviceInternal::CreateTimelineSemaphore(const auto& resources,
    TimelineSemaphoreCreateInfo&& createInfo, DeletionQueue& deletionQueue)
{
    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = createInfo.InitialValue;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    TimelineSemaphoreResource semaphoreResource = {};
    vkCreateSemaphore(g_State.Device, &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore);
    semaphoreResource.Timeline = createInfo.InitialValue;

    TimelineSemaphore semaphore = resources.Add(semaphoreResource);
    deletionQueue.Enqueue(semaphore);

    return semaphore;
}

void DeviceInternal::Destroy(const auto& resources, TimelineSemaphore semaphore)
{
    vkDestroySemaphore(g_State.Device, resources[semaphore].Semaphore, nullptr);
    resources.Remove(semaphore);
}

void DeviceInternal::TimelineSemaphoreWaitCPU(const auto& resources, TimelineSemaphore semaphore, u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &resources[semaphore].Semaphore;
    waitInfo.pValues = &value;

    deviceCheck(vkWaitSemaphores(g_State.Device, &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void DeviceInternal::TimelineSemaphoreSignalCPU(const auto& resources, TimelineSemaphore semaphore, u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = resources[semaphore].Semaphore;
    signalInfo.value = value;

    deviceCheck(vkSignalSemaphore(g_State.Device, &signalInfo),
        "Failed to signal semaphore");

    resources[semaphore].Timeline = value;
}

DependencyInfo DeviceInternal::CreateDependencyInfo(const auto& resources, DependencyInfoCreateInfo&& createInfo,
    DeletionQueue& deletionQueue)
{
    DependencyInfoResource dependencyInfoResource = {};
    dependencyInfoResource.DependencyInfo = {};
    dependencyInfoResource.DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfoResource.DependencyInfo.dependencyFlags = vulkanDependencyFlagsFromPipelineDependencyFlags(
        createInfo.Flags);

    if (createInfo.ExecutionDependencyInfo.has_value())
    {
        VkMemoryBarrier2 memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.ExecutionDependencyInfo->SourceStage);
        memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.ExecutionDependencyInfo->DestinationStage);

        dependencyInfoResource.MemoryBarriers[dependencyInfoResource.MemoryBarriersCount++] = memoryBarrier;
    }
    if (createInfo.MemoryDependencyInfo.has_value())
    {
        VkMemoryBarrier2 memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.MemoryDependencyInfo->SourceStage);
        memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.MemoryDependencyInfo->DestinationStage);
        memoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(
            createInfo.MemoryDependencyInfo->SourceAccess);
        memoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(
            createInfo.MemoryDependencyInfo->DestinationAccess);

        dependencyInfoResource.MemoryBarriers[dependencyInfoResource.MemoryBarriersCount++] = memoryBarrier;
    }
    if (createInfo.LayoutTransitionInfo.has_value())
    {
        const ImageResource& image =
            resources[createInfo.LayoutTransitionInfo->ImageSubresource.Image];
        VkImageMemoryBarrier2 imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageMemoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.LayoutTransitionInfo->SourceStage);
        imageMemoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
            createInfo.LayoutTransitionInfo->DestinationStage);
        imageMemoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(
            createInfo.LayoutTransitionInfo->SourceAccess);
        imageMemoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(
            createInfo.LayoutTransitionInfo->DestinationAccess);
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.oldLayout = vulkanImageLayoutFromImageLayout(createInfo.LayoutTransitionInfo->OldLayout);
        imageMemoryBarrier.newLayout = vulkanImageLayoutFromImageLayout(createInfo.LayoutTransitionInfo->NewLayout);
        imageMemoryBarrier.image = resources[createInfo.LayoutTransitionInfo->ImageSubresource.Image].Image;
        imageMemoryBarrier.subresourceRange = {
            .aspectMask = vulkanImageAspectFromImageUsage(image.Description.Usage),
            .baseMipLevel = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.MipmapBase,
            .levelCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Mipmaps,
            .baseArrayLayer = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.LayerBase,
            .layerCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Layers
        };

        dependencyInfoResource.LayoutDependency = imageMemoryBarrier;
    }

    DependencyInfo dependencyInfo = resources.Add(dependencyInfoResource);
    deletionQueue.Enqueue(dependencyInfo);

    return dependencyInfo;
}

void DeviceInternal::Destroy(const auto& resources, DependencyInfo dependencyInfo)
{
    resources.Remove(dependencyInfo);
}

SplitBarrier DeviceInternal::CreateSplitBarrier(const auto& resources, DeletionQueue& deletionQueue)
{
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    SplitBarrierResource splitBarrierResource = {};
    deviceCheck(vkCreateEvent(g_State.Device, &eventCreateInfo, nullptr, &splitBarrierResource.Event),
        "Failed to create split barrier");

    SplitBarrier splitBarrier = resources.Add(splitBarrierResource);
    deletionQueue.Enqueue(splitBarrier);

    return splitBarrier;
}

void DeviceInternal::Destroy(const auto& resources, SplitBarrier splitBarrier)
{
    vkDestroyEvent(g_State.Device, resources[splitBarrier].Event, nullptr);
    resources.Remove(splitBarrier);
}

ImmediateSubmitContext DeviceInternal::StartSubmitContext(const auto& resources)
{
    {
        std::scoped_lock lock(g_State.SubmitContextMutex);

        if (!g_State.SubmitContexts.empty())
        {
            ImmediateSubmitContext ctx = g_State.SubmitContexts.back();
            g_State.SubmitContexts.pop_back();
            BeginCommandBuffer(resources, ctx.CommandBuffer);

            return ctx;
        }
    }

    ImmediateSubmitContext ctx = {};
    ctx.CommandPool = CreateCommandPool(resources, {.QueueKind = QueueKind::Graphics},
        Device::DummyDeletionQueue());
    ctx.CommandBuffer = CreateCommandBuffer(resources, {
        .Pool = ctx.CommandPool,
        .Kind = CommandBufferKind::Primary
    });
    ctx.CommandList.SetCommandBuffer(ctx.CommandBuffer);
    ctx.Fence = CreateFence(resources, {}, Device::DummyDeletionQueue());
    ctx.QueueKind = QueueKind::Graphics;
    BeginCommandBuffer(resources, ctx.CommandBuffer);

    return ctx;
}

void DeviceInternal::EndSubmitContext(const auto& resources, const ImmediateSubmitContext& ctx)
{
    EndCommandBuffer(resources, ctx.CommandBuffer);
    SubmitCommandBuffer(resources, ctx.CommandBuffer, ctx.QueueKind, ctx.Fence);
    WaitForFence(resources, ctx.Fence);
    ResetFence(resources, ctx.Fence);
    ResetPool(resources, ctx.CommandPool);

    std::scoped_lock lock(g_State.SubmitContextMutex);

    g_State.SubmitContexts.push_back(ctx);
}

template <typename LockedView, typename Fn>
void DeviceInternal::ImmediateSubmit(LockedView& resources, Fn&& uploadFunction)
{
    View<FenceTag, CommandBufferTag, CommandPoolTag> submitResourcesView = resources;
    auto ctx = StartSubmitContext(submitResourcesView);
    uploadFunction(ctx.CommandList);
    EndSubmitContext(submitResourcesView, ctx);
}

#ifdef DESCRIPTOR_BUFFER
u32 DeviceInternal::GetDescriptorSizeBytes(DescriptorType type)
{
    auto& props = g_State.GPUDescriptorBufferProperties;
    switch (type)
    {
    case DescriptorType::Sampler: return (u32)props.samplerDescriptorSize;
    case DescriptorType::Image: return (u32)props.sampledImageDescriptorSize;
    case DescriptorType::ImageStorage: return (u32)props.storageImageDescriptorSize;
    case DescriptorType::TexelUniform: return (u32)props.uniformTexelBufferDescriptorSize;
    case DescriptorType::TexelStorage: return (u32)props.storageTexelBufferDescriptorSize;
    case DescriptorType::UniformBuffer: return (u32)props.uniformBufferDescriptorSize;
    case DescriptorType::StorageBuffer: return (u32)props.storageBufferDescriptorSize;
    case DescriptorType::Input: return (u32)props.inputAttachmentDescriptorSize;
    default:
        return 0;
    }
}

void DeviceInternal::WriteDescriptor(const auto& resources, Descriptors descriptors, DescriptorSlotInfo slotInfo,
    u32 index, VkDescriptorGetInfoEXT& descriptorGetInfo)
{
    auto&& [slot, type] = slotInfo;
    const DescriptorsResource& descriptorsResource = resources[descriptors];
    const u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    const u64 innerOffsetBytes = descriptorSizeBytes * index;
    ASSERT(innerOffsetBytes + descriptorSizeBytes <= descriptorsResource.SizeBytes,
        "Trying to write descriptor outside of the allocated region")

    const u64 offsetBytes = descriptorsResource.Offsets[slot] + innerOffsetBytes;
    const DescriptorArenaAllocatorResource& allocatorResource =
        resources[descriptorsResource.Allocator];
    vkGetDescriptorEXT(g_State.Device, &descriptorGetInfo, descriptorSizeBytes,
        (u8*)allocatorResource.MappedAddress + offsetBytes);
}
void DeviceInternal::BindDescriptors(const auto& resources, CommandBuffer cmd, PipelineLayout pipelineLayout,
    Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    const DescriptorsResource& descriptorsResource = resources[descriptors];
    const DescriptorArenaAllocatorResource& allocatorResource =
        resources[descriptorsResource.Allocator];

    u64 offset = descriptorsResource.Offsets.front();
    vkCmdSetDescriptorBufferOffsetsEXT(resources[cmd].CommandBuffer, bindPoint,
        resources[pipelineLayout].Layout, firstSet, 1, &allocatorResource.DescriptorSet, &offset);
}
#else

u32 DeviceInternal::GetFreePoolIndexFromAllocator(const auto& resources, DescriptorArenaAllocator allocator,
    DescriptorPoolFlags poolFlags)
{
    DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
    for (u32 i = 0; i < allocatorResource.FreePools.size(); i++)
        if (allocatorResource.FreePools[i].Flags == poolFlags)
            return i;

    const u32 index = (u32)allocatorResource.FreePools.size();
    std::vector<VkDescriptorPoolSize> sizes(allocatorResource.PoolSizes.size());
    for (u32 i = 0; i < sizes.size(); i++)
        sizes[i] = {
            .type = vulkanDescriptorTypeFromDescriptorType(allocatorResource.PoolSizes[i].DescriptorType),
            .descriptorCount =
            (u32)(allocatorResource.PoolSizes[i].SetSizeMultiplier * (f32)allocatorResource.MaxSetsPerPool)
        };

    VkDescriptorPool pool = {};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = allocatorResource.MaxSetsPerPool;
    poolCreateInfo.poolSizeCount = (u32)sizes.size();
    poolCreateInfo.pPoolSizes = sizes.data();
    poolCreateInfo.flags = vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(poolFlags);

    deviceCheck(vkCreateDescriptorPool(g_State.Device, &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    allocatorResource.FreePools.push_back({.Pool = pool, .Flags = poolFlags});

    return index;
}

void DeviceInternal::BindDescriptors(const auto& resources, CommandBuffer cmd, PipelineLayout pipelineLayout,
    Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    const DescriptorsResource& descriptorsResource = resources[descriptors];
    vkCmdBindDescriptorSets(resources[cmd].CommandBuffer, bindPoint,
        resources[pipelineLayout].Layout, firstSet, 1, &descriptorsResource.DescriptorSet, 0, nullptr);
}
#endif

std::vector<VkSemaphoreSubmitInfo> DeviceInternal::CreateVulkanSemaphoreSubmit(const auto& resources,
    Span<const Semaphore> semaphores, Span<const PipelineStage> waitStages)
{
    std::vector waitSemaphoreSubmitInfos(semaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(semaphores))
    {
        waitSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreSubmitInfos[i].semaphore = resources[semaphore].Semaphore;
        waitSemaphoreSubmitInfos[i].stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i]);
    }

    return waitSemaphoreSubmitInfos;
}

std::vector<VkSemaphoreSubmitInfo> DeviceInternal::CreateVulkanSemaphoreSubmit(const auto& resources,
    Span<const TimelineSemaphore> semaphores, Span<const u64> waitValues, Span<const PipelineStage> waitStages)
{
    std::vector waitSemaphoreSubmitInfos(semaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(semaphores))
    {
        waitSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreSubmitInfos[i].semaphore = resources[semaphore].Semaphore;
        waitSemaphoreSubmitInfos[i].value = waitValues[i];
        waitSemaphoreSubmitInfos[i].stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i]);
    }

    return waitSemaphoreSubmitInfos;
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const ExecuteSecondaryBufferCommand& command)
{
    vkCmdExecuteCommands(resources[cmd].CommandBuffer, 1, &resources[command.Cmd].CommandBuffer);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const PrepareSwapchainPresentCommand& command)
{
    SwapchainResource& swapchainResource = resources[command.Swapchain];

    ImageSubresource drawSubresource = {
        .Image = swapchainResource.Description.DrawImage,
        .Description = {.Mipmaps = 1, .Layers = 1}
    };
    ImageSubresource presentSubresource = {
        .Image = swapchainResource.Description.ColorImages[command.ImageIndex],
        .Description = {.Mipmaps = 1, .Layers = 1}
    };
    ::DeletionQueue& deletionQueue = *g_State.FrameDeletionQueue;

    LayoutTransitionInfo presentToDestinationTransitionInfo = {
        .ImageSubresource = presentSubresource,
        .SourceStage = PipelineStage::ColorOutput,
        .DestinationStage = PipelineStage::Bottom,
        .SourceAccess = PipelineAccess::ReadColorAttachment | PipelineAccess::WriteColorAttachment,
        .DestinationAccess = PipelineAccess::None,
        .OldLayout = ImageLayout::Undefined,
        .NewLayout = ImageLayout::Destination
    };

    LayoutTransitionInfo destinationToPresentTransitionInfo = presentToDestinationTransitionInfo;
    destinationToPresentTransitionInfo.OldLayout = ImageLayout::Destination;
    destinationToPresentTransitionInfo.NewLayout = ImageLayout::Present;

    CompileCommand(resources, cmd, WaitOnBarrierCommand{
        .DependencyInfo = CreateDependencyInfo(resources, {
            .LayoutTransitionInfo = presentToDestinationTransitionInfo
        }, deletionQueue)
    });

    ImageSubregion sourceSubregion = {
        .Mipmap = (u32)drawSubresource.Description.MipmapBase,
        .LayerBase = (u32)drawSubresource.Description.LayerBase,
        .Layers = (u32)drawSubresource.Description.Layers,
        .Top = GetImageDescription(resources, swapchainResource.Description.DrawImage).Dimensions()
    };

    ImageSubregion destinationSubregion = {
        .Mipmap = (u32)presentSubresource.Description.MipmapBase,
        .LayerBase = (u32)presentSubresource.Description.LayerBase,
        .Layers = (u32)presentSubresource.Description.Layers,
        .Top = GetImageDescription(resources, swapchainResource.Description.ColorImages[command.ImageIndex]).
        Dimensions()
    };

    CompileCommand(resources, cmd, BlitImageCommand{
        .Source = swapchainResource.Description.DrawImage,
        .Destination = swapchainResource.Description.ColorImages[command.ImageIndex],
        .Filter = ImageFilter::Linear,
        .SourceSubregion = sourceSubregion,
        .DestinationSubregion = destinationSubregion
    });

    CompileCommand(resources, cmd, WaitOnBarrierCommand{
        .DependencyInfo = CreateDependencyInfo(resources, {
            .LayoutTransitionInfo = destinationToPresentTransitionInfo
        }, deletionQueue)
    });
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BeginRenderingCommand& command)
{
    const RenderingInfoResource& renderingInfoResource = resources[command.RenderingInfo];

    VkRenderingInfo renderingInfoVulkan = {};
    renderingInfoVulkan.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfoVulkan.layerCount = 1;
    renderingInfoVulkan.renderArea = VkRect2D{
        .offset = {},
        .extent = {renderingInfoResource.RenderArea.x, renderingInfoResource.RenderArea.y}
    };
    renderingInfoVulkan.colorAttachmentCount = (u32)resources[command.RenderingInfo].ColorAttachments.size();
    renderingInfoVulkan.pColorAttachments = resources[command.RenderingInfo].ColorAttachments.data();
    if (resources[command.RenderingInfo].DepthAttachment.has_value())
        renderingInfoVulkan.pDepthAttachment = resources[command.RenderingInfo].DepthAttachment.operator->();

    vkCmdBeginRendering(resources[cmd].CommandBuffer, &renderingInfoVulkan);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const EndRenderingCommand&)
{
    vkCmdEndRendering(resources[cmd].CommandBuffer);
}

void DeviceInternal::CompileCommand(const auto&, CommandBuffer, const ImGuiBeginCommand&)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const ImGuiEndCommand& command)
{
    ImGui::Render();
    CompileCommand(resources, cmd, BeginRenderingCommand{
        .RenderingInfo = command.RenderingInfo
    });
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), resources[cmd].CommandBuffer);
    CompileCommand(resources, cmd, EndRenderingCommand{});
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BeginConditionalRenderingCommand& command)
{
    VkConditionalRenderingBeginInfoEXT beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.buffer = resources[command.Buffer].Buffer;
    beginInfo.offset = command.Offset;

    vkCmdBeginConditionalRenderingEXT(resources[cmd].CommandBuffer, &beginInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const EndConditionalRenderingCommand&)
{
    vkCmdEndConditionalRenderingEXT(resources[cmd].CommandBuffer);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const SetViewportCommand& command)
{
    const VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)command.Size.x, .height = (f32)command.Size.y,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };

    vkCmdSetViewport(resources[cmd].CommandBuffer, 0, 1, &viewport);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const SetScissorsCommand& command)
{
    const VkRect2D scissor = {
        .offset = {(i32)command.Offset.x, (i32)command.Offset.y},
        .extent = {(u32)command.Size.x, (u32)command.Size.y}
    };

    vkCmdSetScissor(resources[cmd].CommandBuffer, 0, 1, &scissor);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const SetDepthBiasCommand& command)
{
    vkCmdSetDepthBias(resources[cmd].CommandBuffer, command.Constant, 0.0f, command.Slope);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const CopyBufferCommand& command)
{
    VkBufferCopy2 copy = {};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy.size = command.SizeBytes;
    copy.srcOffset = command.SourceOffset;
    copy.dstOffset = command.DestinationOffset;

    const BufferResource& sourceResource = resources[command.Source];
    const BufferResource& destinationResource = resources[command.Destination];
    VkCopyBufferInfo2 copyBufferInfo = {};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = sourceResource.Buffer;
    copyBufferInfo.dstBuffer = destinationResource.Buffer;
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copy;

    vkCmdCopyBuffer2(resources[cmd].CommandBuffer, &copyBufferInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const CopyBufferToImageCommand& command)
{
    ASSERT(command.ImageSubresource.Mipmaps == 1, "Buffer to image copies one mipmap at a time")

    const ImageResource& imageResource = resources[command.Image];

    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.bufferOffset = command.BufferOffset;
    bufferImageCopy.imageExtent = {
        .width = std::max(1u, imageResource.Description.Width >> command.ImageSubresource.MipmapBase),
        .height = std::max(1u, imageResource.Description.Height >> command.ImageSubresource.MipmapBase),
        .depth = imageResource.Description.GetDepth(command.ImageSubresource.MipmapBase)
    };
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(imageResource.Description.Usage);
    bufferImageCopy.imageSubresource.mipLevel = (u32)(i32)command.ImageSubresource.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = (u32)(i32)command.ImageSubresource.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = (u32)(i32)command.ImageSubresource.Layers;

    const BufferResource& sourceResource = resources[command.Buffer];
    VkCopyBufferToImageInfo2 copyBufferToImageInfo = {};
    copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copyBufferToImageInfo.srcBuffer = sourceResource.Buffer;
    copyBufferToImageInfo.dstImage = resources[command.Image].Image;
    copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyBufferToImageInfo.regionCount = 1;
    copyBufferToImageInfo.pRegions = &bufferImageCopy;

    vkCmdCopyBufferToImage2(resources[cmd].CommandBuffer, &copyBufferToImageInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const CopyImageCommand& command)
{
    const ImageResource& sourceResource = resources[command.Source];
    const ImageResource& destinationResource = resources[command.Destination];

    const glm::uvec3 extentSource = command.SourceSubregion.Top - command.SourceSubregion.Bottom;
    const glm::uvec3 extentDestination = command.DestinationSubregion.Top - command.DestinationSubregion.Bottom;
    ASSERT(extentSource == extentDestination, "Extents of source and destination must match for image copy")

    VkImageCopy2 imageCopy = {};
    VkCopyImageInfo2 copyImageInfo = {};

    imageCopy.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
    imageCopy.extent = VkExtent3D{
        .width = extentSource.x,
        .height = extentSource.y,
        .depth = extentSource.z
    };
    imageCopy.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(sourceResource.Description.Usage);
    imageCopy.srcSubresource.baseArrayLayer = command.SourceSubregion.LayerBase;
    imageCopy.srcSubresource.layerCount = command.SourceSubregion.Layers;
    imageCopy.srcSubresource.mipLevel = command.SourceSubregion.Mipmap;
    imageCopy.srcOffset = VkOffset3D{
        .x = (i32)command.SourceSubregion.Bottom.x,
        .y = (i32)command.SourceSubregion.Bottom.y,
        .z = (i32)command.SourceSubregion.Bottom.z
    };
    imageCopy.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destinationResource.Description.Usage);
    imageCopy.dstSubresource.baseArrayLayer = command.DestinationSubregion.LayerBase;
    imageCopy.dstSubresource.layerCount = command.DestinationSubregion.Layers;
    imageCopy.dstSubresource.mipLevel = command.DestinationSubregion.Mipmap;
    imageCopy.dstOffset = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Bottom.x,
        .y = (i32)command.DestinationSubregion.Bottom.y,
        .z = (i32)command.DestinationSubregion.Bottom.z
    };

    copyImageInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
    copyImageInfo.srcImage = sourceResource.Image;
    copyImageInfo.dstImage = destinationResource.Image;
    copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImageInfo.regionCount = 1;

    copyImageInfo.pRegions = &imageCopy;

    vkCmdCopyImage2(resources[cmd].CommandBuffer, &copyImageInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BlitImageCommand& command)
{
    const ImageResource& sourceResource = resources[command.Source];
    const ImageResource& destinationResource = resources[command.Destination];

    VkImageBlit2 imageBlit = {};
    VkBlitImageInfo2 blitImageInfo = {};

    imageBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    imageBlit.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(sourceResource.Description.Usage);
    imageBlit.srcSubresource.baseArrayLayer = command.SourceSubregion.LayerBase;
    imageBlit.srcSubresource.layerCount = command.SourceSubregion.Layers;
    imageBlit.srcSubresource.mipLevel = command.SourceSubregion.Mipmap;
    imageBlit.srcOffsets[0] = VkOffset3D{
        .x = (i32)command.SourceSubregion.Bottom.x,
        .y = (i32)command.SourceSubregion.Bottom.y,
        .z = (i32)command.SourceSubregion.Bottom.z
    };
    imageBlit.srcOffsets[1] = VkOffset3D{
        .x = (i32)command.SourceSubregion.Top.x,
        .y = (i32)command.SourceSubregion.Top.y,
        .z = (i32)command.SourceSubregion.Top.z
    };

    imageBlit.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destinationResource.Description.Usage);
    imageBlit.dstSubresource.baseArrayLayer = command.DestinationSubregion.LayerBase;
    imageBlit.dstSubresource.layerCount = command.DestinationSubregion.Layers;
    imageBlit.dstSubresource.mipLevel = command.DestinationSubregion.Mipmap;
    imageBlit.dstOffsets[0] = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Bottom.x,
        .y = (i32)command.DestinationSubregion.Bottom.y,
        .z = (i32)command.DestinationSubregion.Bottom.z
    };
    imageBlit.dstOffsets[1] = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Top.x,
        .y = (i32)command.DestinationSubregion.Top.y,
        .z = (i32)command.DestinationSubregion.Top.z
    };

    blitImageInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitImageInfo.srcImage = sourceResource.Image;
    blitImageInfo.dstImage = destinationResource.Image;
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.filter = vulkanFilterFromImageFilter(command.Filter);

    blitImageInfo.pRegions = &imageBlit;

    vkCmdBlitImage2(resources[cmd].CommandBuffer, &blitImageInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const MipmapImageCommand& command)
{
    const ImageResource& imageResource = resources[command.Image];

    i32 width = (i32)imageResource.Description.Width;
    i32 height = (i32)imageResource.Description.Height;
    i32 depth = (i32)imageResource.Description.GetDepth();
    i8 layers = imageResource.Description.GetLayers();

    ::DeletionQueue& deletionQueue = *g_State.FrameDeletionQueue;

    ImageSubresource imageSubresource = {
        .Image = command.Image,
        .Description = {
            .MipmapBase = 0,
            .Mipmaps = 1,
            .LayerBase = 0,
            .Layers = layers
        }
    };

    LayoutTransitionInfo transitionInfo = {
        .ImageSubresource = imageSubresource,
        .SourceStage = PipelineStage::AllCommands,
        .DestinationStage = PipelineStage::AllTransfer,
        .SourceAccess = PipelineAccess::WriteAll,
        .DestinationAccess = PipelineAccess::ReadTransfer,
        .OldLayout = command.Layout,
        .NewLayout = ImageLayout::Source
    };

    CompileCommand(resources, cmd, WaitOnBarrierCommand{
        .DependencyInfo = CreateDependencyInfo(resources, {
            .LayoutTransitionInfo = transitionInfo
        }, deletionQueue)
    });

    for (i8 mip = 1; mip < imageResource.Description.Mipmaps; mip++)
    {
        ImageSubregion sourceSubregion = {
            .Mipmap = (u32)mip - 1,
            .Layers = (u32)layers,
            .Top = {width, height, depth}
        };

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
        depth = std::max(1, depth >> 1);

        ImageSubregion destinationSubregion = {
            .Mipmap = (u32)mip,
            .Layers = (u32)layers,
            .Top = {width, height, depth}
        };

        ImageSubresource mipmapSubresource = {
            .Image = command.Image,
            .Description = {
                .MipmapBase = mip,
                .Mipmaps = 1,
                .Layers = layers
            }
        };

        transitionInfo = {
            .ImageSubresource = mipmapSubresource,
            .SourceStage = PipelineStage::AllTransfer,
            .DestinationStage = PipelineStage::AllTransfer,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteTransfer,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::Destination
        };
        CompileCommand(resources, cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = transitionInfo
            }, deletionQueue)
        });

        CompileCommand(resources, cmd, BlitImageCommand{
            .Source = command.Image,
            .Destination = command.Image,
            .Filter = imageResource.Description.MipmapFilter,
            .SourceSubregion = sourceSubregion,
            .DestinationSubregion = destinationSubregion
        });

        transitionInfo = {
            .ImageSubresource = mipmapSubresource,
            .SourceStage = PipelineStage::AllCommands,
            .DestinationStage = PipelineStage::AllTransfer,
            .SourceAccess = PipelineAccess::WriteAll,
            .DestinationAccess = PipelineAccess::ReadTransfer,
            .OldLayout = ImageLayout::Destination,
            .NewLayout = ImageLayout::Source
        };

        CompileCommand(resources, cmd, WaitOnBarrierCommand{
            .DependencyInfo = CreateDependencyInfo(resources, {
                .LayoutTransitionInfo = transitionInfo
            }, deletionQueue)
        });
    }
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const WaitOnFullPipelineBarrierCommand&)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;

    vkCmdPipelineBarrier2(resources[cmd].CommandBuffer, &dependencyInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const WaitOnBarrierCommand& command)
{
    const DependencyInfoResource& dependencyInfo = resources[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) :
        nullptr;
    vkCmdPipelineBarrier2(resources[cmd].CommandBuffer, &vkDependencyInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const SignalSplitBarrierCommand& command)
{
    const DependencyInfoResource& dependencyInfo = resources[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) :
        nullptr;
    vkCmdSetEvent2(resources[cmd].CommandBuffer, resources[command.SplitBarrier].Event, &vkDependencyInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const WaitOnSplitBarrierCommand& command)
{
    const DependencyInfoResource& dependencyInfo = resources[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) :
        nullptr;
    vkCmdWaitEvents2(resources[cmd].CommandBuffer, 1, &resources[command.SplitBarrier].Event,
        &vkDependencyInfo);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const ResetSplitBarrierCommand& command)
{
    ASSERT(resources[command.DependencyInfo].MemoryBarriersCount != 0, "Invalid reset operation")

    vkCmdResetEvent2(resources[cmd].CommandBuffer, resources[command.SplitBarrier].Event,
        resources[command.DependencyInfo].MemoryBarriers.front().dstStageMask);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BindVertexBuffersCommand& command)
{
    std::vector<VkBuffer> vkBuffers(command.Buffers.size());
    for (u32 i = 0; i < vkBuffers.size(); i++)
        vkBuffers[i] = resources[command.Buffers[i]].Buffer;

    vkCmdBindVertexBuffers(resources[cmd].CommandBuffer, 0, (u32)vkBuffers.size(), vkBuffers.data(),
        command.Offsets.data());
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU32BufferCommand& command)
{
    vkCmdBindIndexBuffer(resources[cmd].CommandBuffer, resources[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT32);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU16BufferCommand& command)
{
    vkCmdBindIndexBuffer(resources[cmd].CommandBuffer, resources[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT16);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BindIndexU8BufferCommand& command)
{
    vkCmdBindIndexBuffer(resources[cmd].CommandBuffer, resources[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT8_EXT);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindPipelineGraphicsCommand& command)
{
    vkCmdBindPipeline(resources[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        resources[command.Pipeline].Pipeline);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const BindPipelineComputeCommand& command)
{
    vkCmdBindPipeline(resources[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        resources[command.Pipeline].Pipeline);
}

#ifdef DESCRIPTOR_BUFFER
void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindImmutableSamplersGraphicsCommand& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(resources[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, resources[command.PipelineLayout].Layout, command.Set);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindImmutableSamplersComputeCommand& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(resources[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, resources[command.PipelineLayout].Layout, command.Set);
}
#else // DESCRIPTOR_BUFFER
void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindImmutableSamplersGraphicsCommand& command)
{
    vkCmdBindDescriptorSets(resources[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, resources[command.PipelineLayout].Layout, command.Set, 1,
        &resources[command.Descriptors].DescriptorSet, 0, nullptr);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindImmutableSamplersComputeCommand& command)
{
    vkCmdBindDescriptorSets(resources[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, resources[command.PipelineLayout].Layout, command.Set, 1,
        &resources[command.Descriptors].DescriptorSet, 0, nullptr);
}
#endif // DESCRIPTOR_BUFFER

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindDescriptorsGraphicsCommand& command)
{
    BindDescriptors(resources, cmd, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindDescriptorsComputeCommand& command)
{
    BindDescriptors(resources, cmd, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_COMPUTE);
}

#ifdef DESCRIPTOR_BUFFER
void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const BindDescriptorArenaAllocatorsCommand& command)
{
    std::vector<VkDescriptorBufferBindingInfoEXT> descriptorBufferBindings;
    descriptorBufferBindings.reserve(command.Allocators->m_TransientAllocators.size());

    for (auto& allocator : command.Allocators->m_TransientAllocators)
    {
        const DescriptorArenaAllocatorResource& allocatorResource = resources[allocator];
        const u64 deviceAddress = allocatorResource.DeviceAddress;

        VkDescriptorBufferBindingInfoEXT binding = {};
        binding.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        binding.address = deviceAddress;
        binding.usage = allocatorResource.DescriptorBufferUsage;

        descriptorBufferBindings.push_back(binding);
    }

    vkCmdBindDescriptorBuffersEXT(resources[cmd].CommandBuffer, (u32)descriptorBufferBindings.size(),
        descriptorBufferBindings.data());
}
#else // DESCRIPTOR_BUFFER
void DeviceInternal::CompileCommand(const auto&, CommandBuffer, const BindDescriptorArenaAllocatorsCommand&)
{
}
#endif // DESCRIPTOR_BUFFER

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const PushConstantsCommand& command)
{
    const PipelineLayoutResource& layout = resources[command.PipelineLayout];
    const VkPushConstantRange& pushConstantRange = layout.PushConstants.front();
    vkCmdPushConstants(resources[cmd].CommandBuffer, layout.Layout,
        pushConstantRange.stageFlags, 0, pushConstantRange.size, command.Data.data());
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const DrawCommand& command)
{
    vkCmdDraw(resources[cmd].CommandBuffer, command.VertexCount, 1, 0, command.BaseInstance);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const DrawIndexedCommand& command)
{
    vkCmdDrawIndexed(resources[cmd].CommandBuffer, command.IndexCount, 1, 0, 0, command.BaseInstance);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const DrawIndexedIndirectCommand& command)
{
    vkCmdDrawIndexedIndirect(resources[cmd].CommandBuffer, resources[command.Buffer].Buffer,
        command.Offset, command.Count, command.Stride);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const DrawIndexedIndirectCountCommand& command)
{
    vkCmdDrawIndexedIndirectCount(resources[cmd].CommandBuffer,
        resources[command.DrawBuffer].Buffer, command.DrawOffset,
        resources[command.CountBuffer].Buffer, command.CountOffset,
        command.MaxCount, command.Stride);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd, const DispatchCommand& command)
{
    const glm::uvec3 groupSize = (command.Invocations + command.GroupSize - glm::uvec3{1}) / command.GroupSize;
    vkCmdDispatch(resources[cmd].CommandBuffer, groupSize.x, groupSize.y, groupSize.z);
}

void DeviceInternal::CompileCommand(const auto& resources, CommandBuffer cmd,
    const DispatchIndirectCommand& command)
{
    vkCmdDispatchIndirect(resources[cmd].CommandBuffer, resources[command.Buffer].Buffer, command.Offset);
}
