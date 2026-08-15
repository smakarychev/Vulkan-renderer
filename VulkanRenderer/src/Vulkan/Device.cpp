#include "rendererpch.h"

#include "Device.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "DeviceResourceList.h"

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

namespace
{
template <typename Tag>
struct NativeHandleTraits
{
    static_assert(!sizeof(Tag), "No match for type");
};
template <>
struct NativeHandleTraits<SamplerTag>
{
    using Vk = VkSampler;
};
template <>
struct NativeHandleTraits<CommandPoolTag>
{
    using Vk = VkCommandPool;
};
template <>
struct NativeHandleTraits<CommandBufferTag>
{
    using Vk = VkCommandBuffer;
};
template <>
struct NativeHandleTraits<DescriptorsLayoutTag>
{
    using Vk = VkDescriptorSetLayout;
};
template <>
struct NativeHandleTraits<PipelineTag>
{
    using Vk = VkPipeline;
};
template <>
struct NativeHandleTraits<ShaderModuleTag>
{
    using Vk = VkShaderModule;
};
template <>
struct NativeHandleTraits<FenceTag>
{
    using Vk = VkFence;
};
template <>
struct NativeHandleTraits<SemaphoreTag>
{
    using Vk = VkSemaphore;
};
template <>
struct NativeHandleTraits<TimelineSemaphoreTag>
{
    using Vk = VkSemaphore;
};
template <>
struct NativeHandleTraits<SplitBarrierTag>
{
    using Vk = VkEvent;
};
struct SwapchainResource
{
    struct HotData
    {
        using ObjectType = SwapchainTag;
        VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
        VkFormat ColorFormat{};
    };
    struct ColdData
    {
        SwapchainDescription Description{};
        std::vector<Semaphore> RenderSemaphores{};
    };
    HotData Hot{};
    ColdData Cold{};
};

struct BufferResource
{
    using ObjectType = BufferTag;
    VkBuffer Buffer{VK_NULL_HANDLE};
    void* HostAddress{nullptr};
    u64 DeviceAddress{0};
    BufferDescription Description{};
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

    struct HotData
    {
        using ObjectType = ImageTag;
        VkImage Image{VK_NULL_HANDLE};
        ViewsInfo Views{};
        u32 Width{0};
        u32 Height{0};
        u32 LayersDepth{1};
        i8 Mipmaps{1};
        Format Format{Format::Undefined};
        ImageKind Kind{ImageKind::Image2d};
        ImageUsage Usage{ImageUsage::None};
        ImageFilter MipmapFilter{ImageFilter::Linear};
    };
    struct ColdData
    {
        std::vector<ImageSubresourceDescription> AdditionalViews{};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    HotData Hot{};
    ColdData Cold{};
};

#ifdef DESCRIPTOR_BUFFER
struct DescriptorsResource
{
    struct HotData
    {
        using ObjectType = DescriptorsTag;
    };
    struct ColdData
    {
        std::vector<u64> Offsets{};
        u64 SizeBytes{0};
        DescriptorArenaAllocator Allocator{};
    };
    HotData Hot{};
    ColdData Cold{};
};
struct DescriptorArenaAllocatorResource
{
    struct HotData
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
    };
    struct ColdData
    {
        std::vector<Descriptors> Descriptors;
    };
    HotData Hot{};
    ColdData Cold{};
};
#else
struct DescriptorArenaAllocatorResource
{
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
    
    struct HotData
    {
        using ObjectType = DescriptorArenaAllocatorTag;
    };
    struct ColdData
    {
        std::vector<PoolInfo> FreePools;
        std::vector<PoolInfo> UsedPools;
        static constexpr std::array POOL_SIZES = {
            PoolSize{DescriptorType::Sampler, 0.5f},
            PoolSize{DescriptorType::Image, 4.0f},
            PoolSize{DescriptorType::ImageStorage, 1.0f},
            PoolSize{DescriptorType::TexelUniform, 1.0f},
            PoolSize{DescriptorType::TexelStorage, 1.0f},
            PoolSize{DescriptorType::UniformBuffer, 2.0f},
            PoolSize{DescriptorType::StorageBuffer, 2.0f},
            PoolSize{DescriptorType::UniformBufferDynamic, 1.0f},
            PoolSize{DescriptorType::StorageBufferDynamic, 1.0f},
            PoolSize{DescriptorType::Input, 0.5f}
        };
        u32 MaxSetsPerPool{};
    };
    HotData Hot{};
    ColdData Cold{};
};

template <>
struct NativeHandleTraits<DescriptorsTag>
{
    using Vk = VkDescriptorSet;
};
#endif
struct PipelineLayoutResource
{
    struct HotData
    {
        using ObjectType = PipelineLayoutTag;
        VkPipelineLayout Layout{VK_NULL_HANDLE};
    };
    struct ColdData
    {
        std::vector<VkPushConstantRange> PushConstants;
    };
    HotData Hot{};
    ColdData Cold{};
};
}

DeviceResources& deviceResources();

class DeviceResources
{
    FRIEND_INTERNAL
    friend class DeviceInternal;
    friend struct DeviceState;
private:
    template <typename Tag>
    auto& GetResourceList()
    {
        if constexpr (std::is_same_v<Tag, SwapchainTag>)
            return m_Swapchains;
        else  if constexpr(std::is_same_v<Tag, BufferTag>)
            return m_Buffers;
        else if constexpr(std::is_same_v<Tag, BufferArenaTag>)
            return m_BufferArenas;
        else if constexpr(std::is_same_v<Tag, ImageTag>)
            return m_Images;
        else if constexpr (std::is_same_v<Tag, PipelineLayoutTag>)
            return m_PipelineLayouts;
        else if constexpr (std::is_same_v<Tag, DescriptorArenaAllocatorTag>)
            return m_DescriptorArenaAllocators;
#ifdef DESCRIPTOR_BUFFER
        else if constexpr (std::is_same_v<Tag, DescriptorsTag>)
            return m_Descriptors;
#endif // DESCRIPTOR_BUFFER 
        else
            static_assert(!sizeof(Tag), "No match for type");
        
        std::unreachable();
    }
public:
    template <typename ResourceHot>
    auto Add(ResourceHot&& resource)
    {
        deviceResources().m_AllocatedCount += 1;
        return GetResourceList<typename std::decay_t<ResourceHot>::ObjectType>().Insert(
            std::forward<ResourceHot>(resource));
    }
    
    template <typename ResourceHot, typename ResourceCold>
    auto Add(ResourceHot&& hotData, ResourceCold&& coldData)
    {
        deviceResources().m_AllocatedCount += 1;
        return GetResourceList<typename std::decay_t<ResourceHot>::ObjectType>().Insert(
            std::forward<ResourceHot>(hotData), std::forward<ResourceCold>(coldData));
    }
    
    template <typename Tag>
    auto Remove(lux::DeviceResourceListHandle<Tag> handle)
    {
        deviceResources().m_DeallocatedCount += 1;
        return GetResourceList<Tag>().Erase(handle);
    }
    
    template <typename Tag>
    auto& Hot(lux::DeviceResourceListHandle<Tag> handle)
    {
        return GetResourceList<Tag>().GetHot(handle);
    }
    
    template <typename Tag>
    auto& Cold(lux::DeviceResourceListHandle<Tag> handle)
    {
        return GetResourceList<Tag>().GetCold(handle);
    }
    
    template <typename Tag>
    decltype(auto) Get(lux::DeviceResourceListHandle<Tag> handle)
    {
        return GetResourceList<Tag>().Get(handle);
    }

    template <typename Tag>
    auto NativeHandleToVk(lux::DeviceResourceListHandle<Tag> handle) const
    {
        return (typename NativeHandleTraits<Tag>::Vk)handle.GetId();
    }
    template <typename Tag>
    lux::DeviceResourceListHandle<Tag> VkHandleToNative(auto handle) const
    {
        return lux::DeviceResourceListHandle<Tag>((u64)handle);
    }

    std::atomic<u64> m_AllocatedCount{0};
    std::atomic<u64> m_DeallocatedCount{0};

    lux::DeviceResourceList<SwapchainResource::HotData, SwapchainResource::ColdData> m_Swapchains{"Swapchains"};
    lux::DeviceResourceList<BufferResource> m_Buffers{"Buffers"};
    lux::DeviceResourceList<BufferArenaResource> m_BufferArenas{"BufferArenas"};
    lux::DeviceResourceList<ImageResource::HotData, ImageResource::ColdData> m_Images{"Images"};
    lux::DeviceResourceList<PipelineLayoutResource::HotData, PipelineLayoutResource::ColdData> 
        m_PipelineLayouts{"PipelineLayouts"};
    lux::DeviceResourceList<DescriptorArenaAllocatorResource::HotData, DescriptorArenaAllocatorResource::ColdData>
        m_DescriptorArenaAllocators{"DescriptorArenaAllocators"};
    
#ifdef DESCRIPTOR_BUFFER
    lux::DeviceResourceList<DescriptorsResource::HotData, DescriptorsResource::ColdData> m_Descriptors;
#endif // DESCRIPTOR_BUFFER
};

class DeviceInternal
{
public:
    static VmaAllocator& Allocator();

    static VkCommandBuffer GetProfilerCommandBuffer(ProfilerContext* context);

    static Buffer AllocateBuffer(const BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags allocationFlags);

    static VkImageView CreateVulkanImageView(ImageKind kind, ImageUsage usage,
        const ImageSubresourceDescription& subresourceDescription, VkImage image, VkFormat format);

    static VkRenderingAttachmentInfo CreateVulkanRenderingAttachmentInfo(const RenderingAttachment& attachment);

#ifdef DESCRIPTOR_BUFFER
    static u32 GetDescriptorSizeBytes(DescriptorType type);
    static void WriteDescriptor(Descriptors descriptors, DescriptorSlotInfo slotInfo, u32 index,
        VkDescriptorGetInfoEXT& descriptorGetInfo);
#else
    static u32 GetFreePoolIndexFromAllocator(DescriptorArenaAllocator allocator,
        DescriptorPoolFlags poolFlags);
#endif

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(Span<const Semaphore> semaphores, 
        Span<const PipelineStage> waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(Span<const TimelineSemaphore> semaphores,
        Span<const u64> waitValues, Span<const PipelineStage> waitStages);
    static void BindDescriptors(CommandBuffer cmd, PipelineLayout pipelineLayout,
        Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);
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
    DeletionQueue DeletionQueue;
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
    g_State.FrameDeletionQueue = ctx.DeletionQueue.get();
}

Swapchain Device::CreateSwapchain(SwapchainCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
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
    swapchainResource.Hot.ColorFormat = colorFormat.format;
    deviceCheck(vkCreateSwapchainKHR(g_State.Device, &swapchainCreateInfo, nullptr,
        &swapchainResource.Hot.Swapchain), "Failed to create swapchain");

    const glm::uvec2 swapchainResolution = glm::uvec2{extent.width, extent.height};
    const glm::uvec2 drawResolution = createInfo.DrawResolution.x != 0 ?
        createInfo.DrawResolution :
        swapchainResolution;

    swapchainResource.Cold.Description = {
        .SwapchainResolution = swapchainResolution,
        .DrawResolution = drawResolution,
        .DrawFormat = createInfo.DrawFormat,
        .DepthFormat = createInfo.DepthStencilFormat,
        .DrawImage = CreateEmptyImage({
                .Description = ImageDescription{
                    .Width = drawResolution.x,
                    .Height = drawResolution.y,
                    .Format = createInfo.DrawFormat,
                    .Usage = ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color
                }
            },
            DummyDeletionQueue()),
        .DepthImage = CreateEmptyImage({
                .Description = ImageDescription{
                    .Width = drawResolution.x,
                    .Height = drawResolution.y,
                    .Format = createInfo.DepthStencilFormat,
                    .Usage = ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled
                }
            },
            DummyDeletionQueue())
    };

    swapchainResource.Cold.RenderSemaphores.reserve(imageCount);
    for (u32 i = 0; i < imageCount; i++)
        swapchainResource.Cold.RenderSemaphores.push_back({CreateSemaphore(DummyDeletionQueue())});
    Swapchain swapchain = {deviceResources().Add(swapchainResource.Hot, swapchainResource.Cold)};
    CreateSwapchainImages(swapchain);
    deletionQueue.Enqueue(swapchain);

    return swapchain;
}

void Device::CreateSwapchainImages(Swapchain swapchain)
{
    auto&& [hot, cold] = deviceResources().Get(swapchain);
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(g_State.Device, hot->Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(g_State.Device, hot->Swapchain, &imageCount, images.data());

    std::vector<Image> colorImages(imageCount);

    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        ImageResource imageResource = {
            .Hot = {
                .Image = images[i],
                .Width = cold->Description.SwapchainResolution.x,
                .Height = cold->Description.SwapchainResolution.y,
                .LayersDepth = 1,
                .Mipmaps = 1,
                .Kind = ImageKind::Image2d,
                .Usage = ImageUsage::Destination,
            },
        };
        colorImages[i] = {deviceResources().Add(imageResource.Hot, imageResource.Cold)};
        ImageResource::HotData& image = deviceResources().Hot(colorImages[i]);
        image.Views.ViewType.View = DeviceInternal::CreateVulkanImageView(image.Kind, image.Usage,
            ImageSubresourceDescription{.Mipmaps = 1, .Layers = 1}, image.Image, hot->ColorFormat);
        image.Views.ViewList = &image.Views.ViewType.View;
    }

    cold->Description.ColorImages = colorImages;
}

void Device::DestroySwapchainImages(Swapchain swapchain)
{
    const SwapchainResource::ColdData cold = deviceResources().Cold(swapchain);
    for (const auto& colorImage : cold.Description.ColorImages)
    {
        vkDestroyImageView(g_State.Device, *deviceResources().Hot(colorImage).Views.ViewList, nullptr);
        deviceResources().Remove(colorImage);
    }
    Destroy(cold.Description.DrawImage);
    Destroy(cold.Description.DepthImage);
}

void Device::Destroy(Swapchain swapchain)
{
    DestroySwapchainImages(swapchain);
    vkDestroySwapchainKHR(g_State.Device, deviceResources().Hot(swapchain).Swapchain, nullptr);
    for (Semaphore semaphore : deviceResources().Cold(swapchain).RenderSemaphores)
        Destroy(semaphore);
    deviceResources().Remove(swapchain);
}

u32 Device::AcquireNextImage(Swapchain swapchain, Fence renderFence, Semaphore presentSemaphore)
{
    WaitForFence(renderFence);

    u32 imageIndex;
    const VkResult res = vkAcquireNextImageKHR(g_State.Device, deviceResources().Hot(swapchain).Swapchain,
        10'000'000'000, deviceResources().NativeHandleToVk(presentSemaphore), VK_NULL_HANDLE,
        &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        return INVALID_SWAPCHAIN_IMAGE;

    ASSERT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image")

    ResetFence(renderFence);

    return imageIndex;
}

bool Device::Present(Swapchain swapchain, QueueKind queueKind, u32 imageIndex)
{
    VkSwapchainKHR vkSwapchain = deviceResources().Hot(swapchain).Swapchain;
    VkSemaphore waitSemaphore = 
        deviceResources().NativeHandleToVk(deviceResources().Cold(swapchain).RenderSemaphores[imageIndex]);
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vkSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;

    VkResult result = vkQueuePresentKHR(g_State.Queues.GetQueueByKind(queueKind).Queue, &presentInfo);

    ASSERT(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR,
        "Failed to present image")

    return result == VK_SUCCESS;
}

SwapchainDescription Device::GetSwapchainDescription(Swapchain swapchain)
{
    return deviceResources().Cold(swapchain).Description;
}

Semaphore Device::GetSwapchainRenderSemaphore(Swapchain swapchain, u32 imageIndex)
{
    return deviceResources().Cold(swapchain).RenderSemaphores[imageIndex];
}

CommandPool Device::CreateCommandPool(CommandPoolCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkCommandPoolCreateFlags flags = 0;
    if (createInfo.PerBufferReset)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = flags;
    poolCreateInfo.queueFamilyIndex = g_State.Queues.GetFamilyByKind(createInfo.QueueKind);

    VkCommandPool vkCommandPool;
    deviceCheck(vkCreateCommandPool(g_State.Device, &poolCreateInfo, nullptr, &vkCommandPool),
        "Failed to create command pool");

    const CommandPool commandPool = {deviceResources().VkHandleToNative<CommandPoolTag>(vkCommandPool)};
    deletionQueue.Enqueue(commandPool);

    return commandPool;
}

void Device::Destroy(CommandPool pool)
{
    vkDestroyCommandPool(g_State.Device, deviceResources().NativeHandleToVk(pool), nullptr);
}

void Device::ResetPool(CommandPool pool)
{
    deviceCheck(vkResetCommandPool(g_State.Device, deviceResources().NativeHandleToVk(pool), 0),
        "Error while resetting command pool");
}

CommandBuffer Device::CreateCommandBuffer(CommandBufferCreateInfo&& createInfo)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = deviceResources().NativeHandleToVk(createInfo.Pool);
    allocateInfo.level = vulkanBufferLevelFromBufferKind(createInfo.Kind);
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer vkCommandBuffer;
    deviceCheck(vkAllocateCommandBuffers(g_State.Device, &allocateInfo, &vkCommandBuffer),
        "Failed to allocate command buffer");

    const CommandBuffer cmd = {deviceResources().VkHandleToNative<CommandBufferTag>(vkCommandBuffer)};

    return cmd;
}

void Device::ResetCommandBuffer(CommandBuffer cmd)
{
    deviceCheck(vkResetCommandBuffer(deviceResources().NativeHandleToVk(cmd), 0), "Error while resetting command buffer");
}

void Device::BeginCommandBuffer(CommandBuffer cmd)
{
    BeginCommandBuffer(cmd, CommandBufferUsage::SingleSubmit);
}

void Device::BeginCommandBuffer(CommandBuffer cmd, CommandBufferUsage usage)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = vulkanCommandBufferFlagsFromUsage(usage);
    
    deviceCheck(vkBeginCommandBuffer(deviceResources().NativeHandleToVk(cmd), &beginInfo),
        "Error while beginning command buffer");
}

void Device::BeginSecondaryCommandBuffer(CommandBuffer cmd)
{
    BeginSecondaryCommandBuffer(cmd, CommandBufferUsage::SingleSubmit);
}

void Device::BeginSecondaryCommandBuffer(CommandBuffer cmd, CommandBufferUsage usage)
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = vulkanCommandBufferFlagsFromUsage(usage);
    beginInfo.pInheritanceInfo = &inheritanceInfo;
    deviceCheck(vkBeginCommandBuffer(deviceResources().NativeHandleToVk(cmd), &beginInfo),
        "Error while beginning command buffer");
}

void Device::EndCommandBuffer(CommandBuffer cmd)
{
    deviceCheck(vkEndCommandBuffer(deviceResources().NativeHandleToVk(cmd)), "Error while ending command buffer");
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind, const BufferSubmitSyncInfo& submitSync)
{
    SubmitCommandBuffers({cmd}, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    SubmitCommandBuffers({cmd}, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind, Fence fence)
{
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo.commandBuffer = deviceResources().NativeHandleToVk(cmd);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            fence.HasValue() ? deviceResources().NativeHandleToVk(fence) : VK_NULL_HANDLE),
        "Error while submitting command buffer");
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = deviceResources().NativeHandleToVk(cmd);
    }

    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = deviceResources().NativeHandleToVk(semaphore);
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = DeviceInternal::CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitStages);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            submitSync.Fence.HasValue() ? deviceResources().NativeHandleToVk(submitSync.Fence) : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = deviceResources().NativeHandleToVk(cmd);
    }

    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = deviceResources().NativeHandleToVk(semaphore);
        signalSemaphoreSubmitInfos[i].value = submitSync.SignalValues[i];
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = DeviceInternal::CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitValues, submitSync.WaitStages);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(g_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
            submitSync.Fence.HasValue() ? deviceResources().NativeHandleToVk(submitSync.Fence) : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

Buffer Device::CreateBuffer(BufferCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VmaAllocationCreateFlags flags = 0;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::Mappable))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::MappableRandomAccess))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    if (createInfo.PersistentMapping)
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    const Buffer buffer = DeviceInternal::AllocateBuffer(createInfo, 
        vulkanBufferUsageFromUsage(createInfo.Description.Usage), flags);

    if (!createInfo.InitialData.empty())
    {
        if (enumHasAny(createInfo.Description.Usage, BufferUsage::Mappable | BufferUsage::MappableRandomAccess))
        {
            SetBufferData(buffer, createInfo.InitialData, 0);
        }
        else
        {
            const Buffer stagingBuffer = CreateStagingBuffer(createInfo.InitialData.size());
            SetBufferData(stagingBuffer, createInfo.InitialData, 0);
            ImmediateSubmit([&](CommandBuffer cmd)
            {
                CompileCommand(cmd, CopyBufferCommand{
                    .Source = stagingBuffer,
                    .Destination = buffer,
                    .SizeBytes = createInfo.InitialData.size()
                });
            });
            Destroy(stagingBuffer);
        }
    }

    deletionQueue.Enqueue(buffer);

    return buffer;
}

void Device::Destroy(Buffer buffer)
{
    const BufferResource& bufferResource = deviceResources().Get(buffer);
    vmaDestroyBuffer(DeviceInternal::Allocator(), bufferResource.Buffer, bufferResource.Allocation);
    deviceResources().Remove(buffer);
}

Buffer Device::CreateStagingBuffer(u64 sizeBytes)
{
    return CreateBuffer({
            .Description = {
                .SizeBytes = sizeBytes,
                .Usage = BufferUsage::Staging | BufferUsage::Mappable,
            },
            .PersistentMapping = true
        },
        DummyDeletionQueue());
}

void Device::ResizeBuffer(Buffer buffer, u64 newSize, CommandBuffer cmd, bool copyData)
{
    BufferResource& bufferResource = deviceResources().Get(buffer);
    const auto& description = bufferResource.Description;
    const u64 oldSize = description.SizeBytes;
    if (description.SizeBytes == newSize)
        return;

    Buffer newBuffer = CreateBuffer({
            .Description = {
                .SizeBytes = newSize,
                .Usage = description.Usage,
            },
            .PersistentMapping = bufferResource.HostAddress != nullptr
        },
        *g_State.FrameDeletionQueue);

    /* seems very questionable
     * after this line new Buffer will inherit lifetime of old buffer,
     * and old buffer will be deleted in frame queue
     */
    std::swap(bufferResource, deviceResources().Get(newBuffer));

    /* the source and destination are intentionally swapped */
    if (copyData)
        CompileCommand(cmd, {
            .Source = newBuffer,
            .Destination = buffer,
            .SizeBytes = std::min(oldSize, newSize)
        });
}

void* Device::MapBuffer(Buffer buffer)
{
    void* mappedData;
    vmaMapMemory(DeviceInternal::Allocator(), deviceResources().Get(buffer).Allocation, &mappedData);
    return mappedData;
}

void Device::UnmapBuffer(Buffer buffer)
{
    vmaUnmapMemory(DeviceInternal::Allocator(), deviceResources().Get(buffer).Allocation);
}

void Device::SetBufferData(Buffer buffer, Span<const std::byte> data, u64 offsetBytes)
{
    vmaCopyMemoryToAllocation(DeviceInternal::Allocator(), data.data(), deviceResources().Get(buffer).Allocation, 
        offsetBytes, data.size());
}

void Device::SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    mappedAddress = (void*)((u8*)mappedAddress + offsetBytes);
    std::memcpy(mappedAddress, data.data(), data.size());
}

void* Device::GetBufferMappedAddress(Buffer buffer)
{
    return deviceResources().Get(buffer).HostAddress;
}

usize Device::GetBufferSizeBytes(Buffer buffer)
{
    return deviceResources().Get(buffer).Description.SizeBytes;
}

BufferDescription Device::GetBufferDescription(Buffer buffer)
{
    return deviceResources().Get(buffer).Description;
}

u64 Device::GetBufferDeviceAddress(Buffer buffer)
{
    return deviceResources().Get(buffer).DeviceAddress;
}

BufferUsage Device::GetBufferUsage(Buffer buffer)
{
    return deviceResources().Get(buffer).Description.Usage;
}

BufferArena Device::CreateBufferArena(BufferArenaCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VmaVirtualBlockCreateInfo virtualBlockCreateInfo = {};
    virtualBlockCreateInfo.size = createInfo.VirtualSizeBytes;

    BufferArenaResource bufferArenaResource = {};
    deviceCheck(vmaCreateVirtualBlock(&virtualBlockCreateInfo, &bufferArenaResource.VirtualBlock),
        "Failed to create buffer arena");
    bufferArenaResource.Buffer = createInfo.Buffer;
    bufferArenaResource.VirtualSizeBytes = createInfo.VirtualSizeBytes;

    const BufferArena arena = {deviceResources().Add(bufferArenaResource)};
    deletionQueue.Enqueue(arena);

    return arena;
}

void Device::Destroy(BufferArena arena)
{
    VmaVirtualBlock virtualBlock = deviceResources().Get(arena).VirtualBlock;

    vmaClearVirtualBlock(virtualBlock);
    vmaDestroyVirtualBlock(virtualBlock);

    deviceResources().Remove(arena);
}

void Device::ResizeBufferArenaPhysical(BufferArena arena, u64 newSize, CommandBuffer cmd, bool copyData)
{
    const Buffer buffer = deviceResources().Get(arena).Buffer;
    const u64 oldSize = GetBufferSizeBytes(buffer);
    if (oldSize == newSize)
        return;

    ResizeBuffer(buffer, newSize, cmd, copyData);
}

Buffer Device::GetBufferArenaUnderlyingBuffer(BufferArena arena)
{
    return deviceResources().Get(arena).Buffer;
}

u64 Device::GetBufferArenaSizeBytesPhysical(BufferArena arena)
{
    return GetBufferSizeBytes(GetBufferArenaUnderlyingBuffer(arena));
}

BufferSuballocationResult Device::BufferArenaSuballocate(BufferArena arena, u64 sizeBytes, u32 alignment)
{
    VmaVirtualAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.size = sizeBytes + alignment;
    allocationCreateInfo.alignment = 0;
    // todo: is this ok flag to use?
    allocationCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    const BufferArenaResource bufferArenaResource = deviceResources().Get(arena);
    VmaVirtualAllocation allocation;
    const VkResult allocateResult = vmaVirtualAllocate(bufferArenaResource.VirtualBlock,
        &allocationCreateInfo, &allocation, nullptr);
    if (allocateResult != VK_SUCCESS)
        return std::unexpected(BufferSuballocationError::OutOfVirtualMemory);

    VmaVirtualAllocationInfo allocationInfo = {};
    vmaGetVirtualAllocationInfo(bufferArenaResource.VirtualBlock, allocation, &allocationInfo);

    if (allocationInfo.offset + allocationInfo.size > GetBufferSizeBytes(bufferArenaResource.Buffer))
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

void Device::BufferArenaFree(BufferArena arena, BufferSuballocationHandle suballocation)
{
    vmaVirtualFree(deviceResources().Get(arena).VirtualBlock, (VmaVirtualAllocation)suballocation);
}

Image Device::CreateImage(ImageCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    Image image = {};

    if (std::holds_alternative<const lux::assetlib::ImageAsset*>(createInfo.DataSource))
        image = CreateImageFromAssetFile(createInfo,
            std::get<const lux::assetlib::ImageAsset*>(createInfo.DataSource));
    else if (std::holds_alternative<Span<const std::byte>>(createInfo.DataSource))
        image = CreateImageFromPixels(createInfo, std::get<Span<const std::byte>>(createInfo.DataSource));

    deletionQueue.Enqueue(image);

    return image;
}

void Device::PreprocessImageCreateInfo(ImageCreateInfo& createInfo)
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

Image Device::AllocateImage(ImageCreateInfo& createInfo)
{
    PreprocessImageCreateInfo(createInfo);

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
    allocationInfo.flags = 
        enumHasAny(createInfo.Description.Usage, ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil) ?
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT :
        0;

    ImageResource imageResource = {};
    deviceCheck(vmaCreateImage(DeviceInternal::Allocator(), &imageCreateInfo, &allocationInfo,
        &imageResource.Hot.Image, &imageResource.Cold.Allocation, nullptr),
        "Failed to create image");
    imageResource.Hot.Width = createInfo.Description.Width;
    imageResource.Hot.Height = createInfo.Description.Height;
    imageResource.Hot.LayersDepth = createInfo.Description.LayersDepth;
    imageResource.Hot.Mipmaps = createInfo.Description.Mipmaps;
    imageResource.Hot.Format = createInfo.Description.Format;
    imageResource.Hot.Kind = createInfo.Description.Kind;
    imageResource.Hot.Usage = createInfo.Description.Usage;
    imageResource.Hot.MipmapFilter = createInfo.Description.MipmapFilter;
    imageResource.Cold.AdditionalViews = createInfo.Description.AdditionalViews;

    return {deviceResources().Add(imageResource.Hot, imageResource.Cold)};
}


Image Device::CreateImageFromAssetFile(ImageCreateInfo& createInfo, const lux::assetlib::ImageAsset* asset)
{
    Image image = {};
    Buffer imageBuffer = {};

    ImmediateSubmit([&](CommandBuffer cmd)
    {
        u64 totalSizeBytes = 0;
        for (auto& mip : asset->Header.MipmapSizes)
            for (const u64 layerSize : mip)
                totalSizeBytes += layerSize;

        imageBuffer = CreateBuffer({
            .Description = {
                .SizeBytes = totalSizeBytes,
                .Usage = BufferUsage::Source | BufferUsage::StagingRandomAccess,
            },
            .PersistentMapping = true
        }, DummyDeletionQueue());
        void* imageBufferHost = GetBufferMappedAddress(imageBuffer);

        image = AllocateImage(createInfo);
        CreateViews(ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        ImageSubresource imageSubresource = {
            .Image = image,
            .Description = {.Mipmaps = (i8)asset->Header.Mipmaps, .Layers = (i8)asset->Header.Layers}
        };

        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = LayoutTransitionInfo{
                .ImageSubresource = imageSubresource,
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllTransfer,
                .SourceAccess = PipelineAccess::None,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = ImageLayout::Undefined,
                .NewLayout = ImageLayout::Destination
            }
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
                    (std::byte*)imageBufferHost + offset,
                    asset->MipmapsImageData[mip][layer].data(),
                    size
                );
                offset += size;
                mipSize += size;
            }

            CompileCommand(cmd, {
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
            CompileCommand(cmd, MipmapImageCommand{
                .Image = image,
                .Layout = currentLayout
            });
            currentLayout = ImageLayout::Source;
            imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
        }

        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = LayoutTransitionInfo{
                .ImageSubresource = imageSubresource,
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllTransfer,
                .SourceAccess = PipelineAccess::None,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = currentLayout,
                .NewLayout = ImageLayout::Readonly
            }
        });
    });

    Destroy(imageBuffer);

    return image;
}

Image Device::CreateImageFromPixels(ImageCreateInfo& createInfo, Span<const std::byte> pixels)
{
    if (pixels.empty())
    {
        Image image = AllocateImage(createInfo);
        CreateViews(ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        return image;
    }

    const Buffer imageBuffer = CreateBuffer({
            .Description = {
                .SizeBytes = pixels.size(),
                .Usage = BufferUsage::Source | BufferUsage::Staging,
            },
            .InitialData = pixels
        },
        DummyDeletionQueue());

    const Image image = CreateImageFromBuffer(createInfo, imageBuffer);
    Destroy(imageBuffer);

    return image;
}

Image Device::CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer)
{
    Image image = {};

    ImmediateSubmit([&](CommandBuffer cmd)
    {
        image = AllocateImage(createInfo);
        CreateViews(ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

        ImageSubresource imageSubresource = {.Image = image, .Description = {.Mipmaps = 1, .Layers = 1}};

        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = LayoutTransitionInfo{
                .ImageSubresource = imageSubresource,
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllTransfer,
                .SourceAccess = PipelineAccess::None,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = ImageLayout::Undefined,
                .NewLayout = ImageLayout::Destination
            }
        });

        CompileCommand(cmd, CopyBufferToImageCommand{
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
            CompileCommand(cmd, MipmapImageCommand{
                .Image = image,
                .Layout = currentLayout
            });
            currentLayout = ImageLayout::Source;
            imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
        }

        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = LayoutTransitionInfo{
                .ImageSubresource = imageSubresource,
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllTransfer,
                .SourceAccess = PipelineAccess::None,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = currentLayout,
                .NewLayout = ImageLayout::Readonly
            }
        });
    });

    return image;
}

Image Device::CreateEmptyImage(ImageCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    const Image image = AllocateImage(createInfo);
    CreateViews(ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);

    deletionQueue.Enqueue(image);

    return image;
}

ImageSubresourceDescription Device::GetAdditionalImageView(Image image, u32 viewIndex)
{
    return deviceResources().Cold(image).AdditionalViews[viewIndex];
}

void Device::Destroy(Image image)
{
    const ImageResource::HotData& hotData = deviceResources().Hot(image);
    const bool onlyPrimaryView = hotData.Views.ViewList == &hotData.Views.ViewType.View;
    const ImageResource::ColdData& coldData = deviceResources().Cold(image);
    if (onlyPrimaryView)
    {
        vkDestroyImageView(g_State.Device, hotData.Views.ViewType.View, nullptr);
    }
    else
    {
        for (u32 viewIndex = 0; viewIndex < hotData.Views.ViewType.ViewCount; viewIndex++)
            vkDestroyImageView(g_State.Device, hotData.Views.ViewList[viewIndex], nullptr);
        delete[] hotData.Views.ViewList;
    }
    vmaDestroyImage(DeviceInternal::Allocator(), hotData.Image, coldData.Allocation);
    deviceResources().Remove(image);
}

void Device::CreateViews(const ImageSubresource& image, const std::vector<ImageSubresourceDescription>& additionalViews)
{
    auto&& [imageHot, imageCold] = deviceResources().Get(image.Image);
    VkFormat viewFormat = vulkanFormatFromFormat(imageHot->Format);
    if (additionalViews.empty())
    {
        imageHot->Views.ViewType.View = DeviceInternal::CreateVulkanImageView(
            imageHot->Kind, imageHot->Usage, image.Description, imageHot->Image, viewFormat);
        imageHot->Views.ViewList = &imageHot->Views.ViewType.View;
        return;
    }

    imageHot->Views.ViewType.ViewCount = 1 + (u32)imageCold->AdditionalViews.size();
    imageHot->Views.ViewList = new VkImageView[imageHot->Views.ViewType.ViewCount];
    imageHot->Views.ViewList[0] = DeviceInternal::CreateVulkanImageView(
        imageHot->Kind, imageHot->Usage, image.Description, imageHot->Image, viewFormat);
    for (u32 viewIndex = 0; viewIndex < additionalViews.size(); viewIndex++)
        imageHot->Views.ViewList[viewIndex + 1] = DeviceInternal::CreateVulkanImageView(
            imageHot->Kind, imageHot->Usage, additionalViews[viewIndex], imageHot->Image, viewFormat);
}

ImageViewHandle Device::GetImageViewHandle(Image image, ImageSubresourceDescription subresourceDescription)
{
    if (subresourceDescription == ImageSubresourceDescription{})
        return 0;
    
    const ImageResource::ColdData& cold = deviceResources().Cold(image);
    auto it = std::ranges::find(cold.AdditionalViews, subresourceDescription);

    if (it != cold.AdditionalViews.end())
        return ImageViewHandle{u32(it - cold.AdditionalViews.begin()) + 1};

    LUX_LOG_ERROR("Image does not have such view subresource, returning default view");
    
    return ImageViewHandle{};
}

ImageDescription Device::GetImageDescription(Image image)
{
    auto&& [hot, cold] = deviceResources().Get(image);
    return ImageDescription{
        .Width = hot->Width,
        .Height = hot->Height,
        .LayersDepth = hot->LayersDepth,
        .Mipmaps = hot->Mipmaps,
        .Format = hot->Format,
        .Kind = hot->Kind,
        .Usage = hot->Usage,
        .MipmapFilter = hot->MipmapFilter,
        .AdditionalViews = cold->AdditionalViews
    };
}

i8 Device::GetImageMipmaps(Image image)
{
    return deviceResources().Hot(image).Mipmaps;
}

glm::uvec2 Device::GetImageSize2d(Image image)
{
    const ImageResource::HotData& hot = deviceResources().Hot(image);
    
    return {
        hot.Width, hot.Height
    };
}

glm::uvec3 Device::GetImageSize3d(Image image)
{
    const ImageResource::HotData& hot = deviceResources().Hot(image);
    
    return {
        hot.Width, hot.Height, hot.Kind == ImageKind::Image3d ? hot.LayersDepth : 1
    };
}

ImageUsage Device::GetImageUsage(Image image)
{
    return deviceResources().Hot(image).Usage;
}

Sampler Device::CreateSampler(SamplerCreateInfo&& createInfo)
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
    samplerCreateInfo.maxAnisotropy = GetAnisotropyLevel();
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

    VkSampler vkSampler;
    deviceCheck(vkCreateSampler(g_State.Device, &samplerCreateInfo, nullptr, &vkSampler),
        "Failed to create depth pyramid sampler");

    const Sampler sampler = deviceResources().VkHandleToNative<SamplerTag>(vkSampler);
    DeletionQueue().Enqueue(sampler);

    SamplerCache::Emplace(key, sampler);

    return sampler;
}

void Device::Destroy(Sampler sampler)
{
    vkDestroySampler(g_State.Device, deviceResources().NativeHandleToVk(sampler), nullptr);
}

PipelineLayout Device::CreatePipelineLayout(PipelineLayoutCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
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
        descriptorsLayouts.push_back(deviceResources().NativeHandleToVk(descriptorLayout));

    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pushConstantRangeCount = (u32)pushConstantRanges.size();
    layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
    layoutCreateInfo.setLayoutCount = (u32)descriptorsLayouts.size();
    layoutCreateInfo.pSetLayouts = descriptorsLayouts.data();

    PipelineLayoutResource pipelineLayoutResource = {};
    pipelineLayoutResource.Cold.PushConstants = pushConstantRanges;
    deviceCheck(vkCreatePipelineLayout(g_State.Device, &layoutCreateInfo, nullptr,
        &pipelineLayoutResource.Hot.Layout), "Failed to create pipeline layout");

    PipelineLayout layout = deviceResources().Add(pipelineLayoutResource.Hot, pipelineLayoutResource.Cold);
    deletionQueue.Enqueue(layout);

    return layout;
}

void Device::Destroy(PipelineLayout pipelineLayout)
{
    vkDestroyPipelineLayout(g_State.Device, deviceResources().Hot(pipelineLayout).Layout, nullptr);
    deviceResources().Remove(pipelineLayout);
}

Pipeline Device::CreatePipeline(PipelineCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkPipelineLayout layout = deviceResources().Hot(createInfo.PipelineLayout).Layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    shaders.reserve(createInfo.Shaders.size());
    for (auto&& [i, shader] : std::views::enumerate(createInfo.Shaders))
    {
        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.stage = vulkanStageBitFromShaderStage(createInfo.ShaderStages[i]);
        shaderStageCreateInfo.module = deviceResources().NativeHandleToVk(shader);
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

        VkPipeline vkPipeline;
        deviceCheck(vkCreateComputePipelines(g_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &vkPipeline), "Failed to create compute pipeline");
        pipeline = deviceResources().VkHandleToNative<PipelineTag>(vkPipeline);
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
            (createInfo.DepthMode == DepthMode::None || createInfo.DepthMode == DepthMode::Read) ?
            VK_FALSE :
            VK_TRUE;
        depthStencilState.depthCompareOp = createInfo.DepthTest == DepthTest::GreaterOrEqual ?
            VK_COMPARE_OP_GREATER_OR_EQUAL :
            VK_COMPARE_OP_EQUAL;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable = VK_FALSE;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(std::max(1u,
            (u32)createInfo.ColorFormats.size()));
        for (auto& attachment : colorBlendAttachmentStates)
        {
            attachment = {};
            attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            switch (createInfo.AlphaBlending)
            {
            case AlphaBlending::None:
                attachment.blendEnable = VK_FALSE;
                break;
            case AlphaBlending::Over:
                attachment.blendEnable = VK_TRUE;
                attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.colorBlendOp = VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                attachment.alphaBlendOp = VK_BLEND_OP_ADD;
                break;
            case AlphaBlending::Additive:
                attachment.blendEnable = VK_TRUE;
                attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.colorBlendOp = VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.alphaBlendOp = VK_BLEND_OP_ADD;
                break;
            default:
                ASSERT(false, "Unsupported blending mode")
            }
        }
        
        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.logicOpEnable = VK_FALSE;
        colorBlendState.attachmentCount = (u32)colorBlendAttachmentStates.size();
        colorBlendState.pAttachments = colorBlendAttachmentStates.data();

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

        VkPipeline vkPipeline;
        deviceCheck(vkCreateGraphicsPipelines(g_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &vkPipeline), "Failed to create graphics pipeline");
        pipeline = deviceResources().VkHandleToNative<PipelineTag>(vkPipeline);
    }
    deletionQueue.Enqueue(pipeline);

    return pipeline;
}

void Device::Destroy(Pipeline pipeline)
{
    vkDestroyPipeline(g_State.Device, deviceResources().NativeHandleToVk(pipeline), nullptr);
}

ShaderModule Device::CreateShaderModule(ShaderModuleCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = createInfo.Source.size();
    moduleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.Source.data());

    VkShaderModule vkShaderModule;
    deviceCheck(vkCreateShaderModule(g_State.Device, &moduleCreateInfo, nullptr, &vkShaderModule),
        "Failed to create shader module");

    const auto module = deviceResources().VkHandleToNative<ShaderModuleTag>(vkShaderModule);
    deletionQueue.Enqueue(module);

    return module;
}

void Device::Destroy(ShaderModule shaderModule)
{
    vkDestroyShaderModule(g_State.Device, deviceResources().NativeHandleToVk(shaderModule), nullptr);
}

DescriptorsLayout Device::CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo)
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
    std::vector<VkSampler> immutableSamplers;
    bindings.reserve(createInfo.Bindings.size());
    immutableSamplers.reserve(createInfo.Bindings.size());
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
        {
            immutableSamplers.push_back(deviceResources().NativeHandleToVk(binding.ImmutableSampler));
            bindings.back().pImmutableSamplers = &immutableSamplers.back();
        }
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

    VkDescriptorSetLayout vkDescriptorSetLayout;
    deviceCheck(vkCreateDescriptorSetLayout(g_State.Device, &layoutCreateInfo, nullptr,
        &vkDescriptorSetLayout), "Failed to create descriptor set layout");

    const auto layout = deviceResources().VkHandleToNative<DescriptorsLayoutTag>(vkDescriptorSetLayout);
    DeletionQueue().Enqueue(layout);

    DescriptorLayoutCache::Emplace(key, layout);

    return layout;
}

DescriptorsLayout Device::GetEmptyDescriptorsLayout()
{
#ifdef DESCRIPTOR_BUFFER
    static const DescriptorsLayout EMPTY_LAYOUT =
        CreateDescriptorsLayout({.Flags = DescriptorLayoutFlags::DescriptorBuffer});
#else
    static const DescriptorsLayout EMPTY_LAYOUT = CreateDescriptorsLayout({});
#endif

    return EMPTY_LAYOUT;
}

void Device::Destroy(DescriptorsLayout layout)
{
    vkDestroyDescriptorSetLayout(g_State.Device, deviceResources().NativeHandleToVk(layout), nullptr);
}

#ifdef DESCRIPTOR_BUFFER
DescriptorArenaAllocator Device::CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo, 
    ::DeletionQueue& deletionQueue)
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
    allocatorResource.Hot.DescriptorSet = createInfo.DescriptorSet;
    allocatorResource.Hot.DescriptorBufferUsage = descriptorBufferUsage;
    allocatorResource.Hot.Residence = createInfo.Residence;
    allocatorResource.Hot.SizeBytes = arenaSizeBytes;
    allocatorResource.Cold.Descriptors.reserve(createInfo.DescriptorCount);
    const BufferCreateInfo arenaCreateInfo = {.Description = {.SizeBytes = arenaSizeBytes}, .PersistentMapping = true};
    allocatorResource.Hot.Arena = DeviceInternal::AllocateBuffer(arenaCreateInfo, usageFlags, allocationFlags);
    allocatorResource.Hot.DeviceAddress = GetBufferDeviceAddress(allocatorResource.Hot.Arena);
    allocatorResource.Hot.MappedAddress = GetBufferMappedAddress(allocatorResource.Hot.Arena);

    DescriptorArenaAllocator allocator = {deviceResources().Add(allocatorResource.Hot, allocatorResource.Cold)};
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void Device::Destroy(DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator( allocator);
    Destroy(deviceResources().Hot(allocator).Arena);
    deviceResources().Remove(allocator);
}

std::optional<Descriptors> Device::AllocateDescriptors(DescriptorArenaAllocator allocator, DescriptorsLayout layout, 
    DescriptorAllocatorAllocationBindings&& bindings)
{
    auto&& [hot, cold] = deviceResources().Get(allocator);
    
    auto& descriptorBufferProps = g_State.GPUDescriptorBufferProperties;

    /* if we have bindless binding, we have to calculate layout size as a sum of bindings sizes */
    u64 layoutSizeBytes = 0;
    if (bindings.BindlessCount == 0)
    {
        vkGetDescriptorSetLayoutSizeEXT(g_State.Device,
            deviceResources().NativeHandleToVk(layout), &layoutSizeBytes);
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
                bindings.BindlessCount * DeviceInternal::GetDescriptorSizeBytes(binding.Type) :
                DeviceInternal::GetDescriptorSizeBytes(binding.Type);
        }
    }

    layoutSizeBytes =
        lux::mem::alignAddressPow2(layoutSizeBytes, (u16)descriptorBufferProps.descriptorBufferOffsetAlignment);
    if (layoutSizeBytes + hot->CurrentOffset > hot->SizeBytes)
        return {};
    hot->CurrentOffset += layoutSizeBytes;

    std::vector<u64> bindingOffsets(bindings.Bindings.size());
    for (u32 offsetIndex = 0; offsetIndex < bindingOffsets.size(); offsetIndex++)
    {
        auto& binding = bindings.Bindings[offsetIndex];
        vkGetDescriptorSetLayoutBindingOffsetEXT(g_State.Device, deviceResources().NativeHandleToVk(layout), 
            binding.Binding, &bindingOffsets[offsetIndex]);
        bindingOffsets[offsetIndex] += hot->CurrentOffset;
    }

    DescriptorsResource descriptorsResource = {};
    descriptorsResource.Cold.Offsets = bindingOffsets;
    descriptorsResource.Cold.SizeBytes = layoutSizeBytes;
    descriptorsResource.Cold.Allocator = allocator;

    Descriptors descriptors = {deviceResources().Add(descriptorsResource.Hot, descriptorsResource.Cold)};
    cold->Descriptors.push_back(descriptors);

    return descriptors;
}

void Device::ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator)
{
    auto&& [hot, cold] = deviceResources().Get(allocator);
    hot->CurrentOffset = 0;
    for (Descriptors descriptors : cold->Descriptors)
        deviceResources().Remove(descriptors);
    cold->Descriptors.clear();
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, const BufferSubresource& buffer, 
    u32 index)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type != DescriptorType::TexelStorage && type != DescriptorType::TexelUniform,
        "Texel buffers require format information")
    ASSERT(type != DescriptorType::StorageBufferDynamic && type != DescriptorType::UniformBufferDynamic,
        "Dynamic buffers are not supported when using descriptor buffer")

    const BufferDescription bufferDescription = deviceResources().Get(buffer.Buffer).Description;
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer =  deviceResources().Get(buffer.Buffer).Buffer;
    u64 deviceAddress = vkGetBufferDeviceAddress(g_State.Device, &deviceAddressInfo);

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.address = deviceAddress + buffer.Description.Offset;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
    ASSERT(
        buffer.Description.SizeBytes <= bufferDescription.SizeBytes ||
        buffer.Description.SizeBytes == BufferSubresourceDescription::WHOLE_SIZE,
        "Buffer subresource size is too large")
    descriptorAddressInfo.range = std::min(buffer.Description.SizeBytes, bufferDescription.SizeBytes);

    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    // using the fact that 'descriptorGetInfo.data' is union
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    DeviceInternal::WriteDescriptor( descriptors, slotInfo, index, descriptorGetInfo);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type == DescriptorType::Sampler)

    VkSampler vkSampler = deviceResources().NativeHandleToVk(sampler);
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    descriptorGetInfo.data.pSampler = &vkSampler;
    DeviceInternal::WriteDescriptor(descriptors, slotInfo, 0, descriptorGetInfo);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, const ImageSubresource& image,
    ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.imageView =
        deviceResources().Hot(image.Image).Views.ViewList[GetImageViewHandle(image.Image, image.Description).m_Index];
    descriptorImageInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);
    descriptorGetInfo.data.pSampledImage = &descriptorImageInfo;

    DeviceInternal::WriteDescriptor(descriptors, slotInfo, index, descriptorGetInfo);
}
#else // DESCRIPTOR_BUFFER
DescriptorArenaAllocator Device::CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    DescriptorArenaAllocatorResource descriptorAllocatorResource = {};
    descriptorAllocatorResource.Cold.MaxSetsPerPool = createInfo.DescriptorCount;

    const DescriptorArenaAllocator allocator = {
        deviceResources().Add(descriptorAllocatorResource.Hot, descriptorAllocatorResource.Cold)
    };
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void Device::Destroy(DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator(allocator);

    const DescriptorArenaAllocatorResource::ColdData& coldData = deviceResources().Cold(allocator);
    for (auto& pool : coldData.FreePools)
        vkDestroyDescriptorPool(g_State.Device, pool.Pool, nullptr);
    for (auto& pool : coldData.UsedPools)
        vkDestroyDescriptorPool(g_State.Device, pool.Pool, nullptr);
    deviceResources().Remove(allocator);
}

std::optional<Descriptors> Device::AllocateDescriptors(DescriptorArenaAllocator allocator, DescriptorsLayout layout,
    DescriptorAllocatorAllocationBindings&& bindings)
{
    DescriptorArenaAllocatorResource::ColdData& cold = deviceResources().Cold(allocator);
    const bool hasBindless = bindings.BindlessCount > 0;
    const DescriptorPoolFlags poolFlags = hasBindless ?
        DescriptorPoolFlags::UpdateAfterBind :
        DescriptorPoolFlags::None;

    u32 poolIndex = DeviceInternal::GetFreePoolIndexFromAllocator(allocator, poolFlags);
    VkDescriptorPool pool = cold.FreePools[poolIndex].Pool;

    VkDescriptorSetVariableDescriptorCountAllocateInfo vulkanVariableBindingCounts = {};
    vulkanVariableBindingCounts.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    vulkanVariableBindingCounts.descriptorSetCount = (u32)hasBindless;
    vulkanVariableBindingCounts.pDescriptorCounts = &bindings.BindlessCount;

    VkDescriptorSetLayout descriptorSetLayout = deviceResources().NativeHandleToVk(layout);
    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &descriptorSetLayout;
    allocateInfo.pNext = &vulkanVariableBindingCounts;

    VkDescriptorSet vkDescriptorSet;
    vkAllocateDescriptorSets(g_State.Device, &allocateInfo, &vkDescriptorSet);

    if (vkDescriptorSet == VK_NULL_HANDLE)
    {
        cold.UsedPools.push_back(cold.FreePools[poolIndex]);
        cold.FreePools.erase(cold.FreePools.begin() + poolIndex);

        poolIndex = DeviceInternal::GetFreePoolIndexFromAllocator(allocator, poolFlags);
        pool = cold.FreePools[poolIndex].Pool;
        allocateInfo.descriptorPool = pool;

        allocateInfo.pSetLayouts = &descriptorSetLayout;
        vkAllocateDescriptorSets(g_State.Device, &allocateInfo, &vkDescriptorSet);
        if (vkDescriptorSet == VK_NULL_HANDLE)
            return std::nullopt;
    }

    const Descriptors set = {deviceResources().VkHandleToNative<DescriptorsTag>(vkDescriptorSet)};

    return set;
}

void Device::ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator)
{
    DescriptorArenaAllocatorResource::ColdData& cold = deviceResources().Cold(allocator);
    for (auto& pool : cold.FreePools)
        vkResetDescriptorPool(g_State.Device, pool.Pool, 0);
    for (auto pool : cold.UsedPools)
    {
        vkResetDescriptorPool(g_State.Device, pool.Pool, 0);
        cold.FreePools.push_back(pool);
    }
    cold.UsedPools.clear();
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = deviceResources().Get(buffer.Buffer).Buffer;
    descriptorBufferInfo.offset = buffer.Description.Offset;
    descriptorBufferInfo.range = buffer.Description.SizeBytes;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = deviceResources().NativeHandleToVk(descriptors);
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pBufferInfo = &descriptorBufferInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = deviceResources().NativeHandleToVk(sampler);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = deviceResources().NativeHandleToVk(descriptors);
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.imageView = deviceResources().Hot(image.Image).Views.ViewList[GetImageViewHandle(
        image.Image, image.Description).m_Index];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = deviceResources().NativeHandleToVk(descriptors);
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(g_State.Device, 1, &write, 0, nullptr);
}
#endif // DESCRIPTOR_BUFFER


Fence Device::CreateFence(FenceCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (createInfo.IsSignaled)
        fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        fenceCreateInfo.flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence vkFence;
    deviceCheck(vkCreateFence(g_State.Device, &fenceCreateInfo, nullptr, &vkFence),
        "Failed to create fence");

    const Fence fence = {deviceResources().VkHandleToNative<FenceTag>(vkFence)};
    deletionQueue.Enqueue(fence);

    return fence;
}

void Device::Destroy(Fence fence)
{
    vkDestroyFence(g_State.Device, deviceResources().NativeHandleToVk(fence), nullptr);
}

void Device::WaitForFence(Fence fence)
{
    const VkFence vkFence = deviceResources().NativeHandleToVk(fence);
    deviceCheck(vkWaitForFences(g_State.Device, 1, &vkFence, true, 10'000'000'000),
        "Error while waiting for fences");
}

bool Device::CheckFence(Fence fence)
{
    const VkResult result = vkGetFenceStatus(g_State.Device, deviceResources().NativeHandleToVk(fence));
    return result == VK_SUCCESS;
}

void Device::ResetFence(Fence fence)
{
    const VkFence vkFence = deviceResources().NativeHandleToVk(fence);
    deviceCheck(vkResetFences(g_State.Device, 1, &vkFence), "Error while resetting fences");
}

Semaphore Device::CreateSemaphore(::DeletionQueue& deletionQueue)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore vkSemaphore;
    deviceCheck(vkCreateSemaphore(g_State.Device, &semaphoreCreateInfo, nullptr, &vkSemaphore),
        "Failed to create semaphore");

    const auto semaphore = deviceResources().VkHandleToNative<SemaphoreTag>(vkSemaphore);
    deletionQueue.Enqueue(semaphore);

    return semaphore;
}

void Device::Destroy(Semaphore semaphore)
{
    vkDestroySemaphore(g_State.Device, deviceResources().NativeHandleToVk(semaphore), nullptr);
}

TimelineSemaphore Device::CreateTimelineSemaphore(TimelineSemaphoreCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = createInfo.InitialValue;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    VkSemaphore vkSemaphore;
    vkCreateSemaphore(g_State.Device, &semaphoreCreateInfo, nullptr, &vkSemaphore);

    const TimelineSemaphore semaphore = {deviceResources().VkHandleToNative<TimelineSemaphoreTag>(vkSemaphore)};
    deletionQueue.Enqueue(semaphore);

    return semaphore;
}

void Device::Destroy(TimelineSemaphore semaphore)
{
    vkDestroySemaphore(g_State.Device, deviceResources().NativeHandleToVk(semaphore), nullptr);
}

void Device::TimelineSemaphoreWaitCPU(TimelineSemaphore semaphore, u64 value)
{
    VkSemaphore vkSemaphore = deviceResources().NativeHandleToVk(semaphore);
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &vkSemaphore;
    waitInfo.pValues = &value;

    deviceCheck(vkWaitSemaphores(g_State.Device, &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void Device::TimelineSemaphoreSignalCPU(TimelineSemaphore semaphore, u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = deviceResources().NativeHandleToVk(semaphore);
    signalInfo.value = value;

    deviceCheck(vkSignalSemaphore(g_State.Device, &signalInfo), "Failed to signal semaphore");
}

SplitBarrier Device::CreateSplitBarrier(::DeletionQueue& deletionQueue)
{
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    VkEvent vkEvent;
    deviceCheck(vkCreateEvent(g_State.Device, &eventCreateInfo, nullptr, &vkEvent),
        "Failed to create split barrier");

    const SplitBarrier splitBarrier = deviceResources().VkHandleToNative<SplitBarrierTag>(vkEvent);
    deletionQueue.Enqueue(splitBarrier);

    return splitBarrier;
}

void Device::Destroy(SplitBarrier splitBarrier)
{
    vkDestroyEvent(g_State.Device, deviceResources().NativeHandleToVk(splitBarrier), nullptr);
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
            
            VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT shaderInterlockFeaturesExt = {};
            shaderInterlockFeaturesExt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
            shaderInterlockFeaturesExt.pNext = &physicalDeviceDescriptorBufferFeaturesExt;

            VkPhysicalDeviceFeatures2 features = {};
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features.pNext = &shaderInterlockFeaturesExt;

            vkGetPhysicalDeviceFeatures2(gpu, &features);

            bool suitable = features.features.samplerAnisotropy == VK_TRUE &&
                features.features.multiDrawIndirect == VK_TRUE &&
                features.features.drawIndirectFirstInstance == VK_TRUE &&
                features.features.geometryShader == VK_TRUE &&
                features.features.shaderSampledImageArrayDynamicIndexing == VK_TRUE &&
                features.features.shaderInt16 == VK_TRUE &&
                features.features.shaderInt64 == VK_TRUE &&
                features.features.depthClamp == VK_TRUE &&
                features.features.fragmentStoresAndAtomics == VK_TRUE &&
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
                physicalDeviceIndexTypeUint8FeaturesExt.indexTypeUint8 == VK_TRUE &&
                shaderInterlockFeaturesExt.fragmentShaderPixelInterlock == VK_TRUE;
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
    deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

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
    
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT shaderInterlockFeaturesExt = {};
    shaderInterlockFeaturesExt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
    shaderInterlockFeaturesExt.pNext = &physicalDeviceDescriptorBufferFeaturesExt;
    shaderInterlockFeaturesExt.fragmentShaderPixelInterlock = VK_TRUE;
    shaderInterlockFeaturesExt.fragmentShaderPixelInterlock = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &shaderInterlockFeaturesExt;
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
    {
        std::scoped_lock lock(g_State.SubmitContextMutex);

        if (!g_State.SubmitContexts.empty())
        {
            ImmediateSubmitContext ctx = g_State.SubmitContexts.back();
            g_State.SubmitContexts.pop_back();
            BeginCommandBuffer(ctx.CommandBuffer);

            return ctx;
        }
    }

    ImmediateSubmitContext ctx = {};
    ctx.CommandPool = CreateCommandPool({.QueueKind = QueueKind::Graphics}, DummyDeletionQueue());
    ctx.CommandBuffer = CreateCommandBuffer({
        .Pool = ctx.CommandPool,
        .Kind = CommandBufferKind::Primary
    });
    ctx.Fence = CreateFence({}, DummyDeletionQueue());
    ctx.QueueKind = QueueKind::Graphics;
    BeginCommandBuffer(ctx.CommandBuffer);

    return ctx;
}

void Device::EndSubmitContext(const ImmediateSubmitContext& ctx)
{
    EndCommandBuffer(ctx.CommandBuffer);
    SubmitCommandBuffer(ctx.CommandBuffer, ctx.QueueKind, ctx.Fence);
    WaitForFence(ctx.Fence);
    ResetFence(ctx.Fence);
    ResetPool(ctx.CommandPool);

    std::scoped_lock lock(g_State.SubmitContextMutex);

    g_State.SubmitContexts.push_back(ctx);
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
    const TracyVkCtx context = TracyVkContext(g_State.GPU, g_State.Device,
        g_State.Queues.Graphics.Queue, deviceResources().NativeHandleToVk(cmd))

    return context;
}

void Device::DestroyTracyGraphicsContext(ProfilerContext::Ctx context)
{
    TracyVkDestroy((TracyVkCtx)context)
}

void Device::CreateGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu, const SourceLocationData& sourceLocationData)
{
    static_assert(sizeof(zoneGpu.Impl) >= sizeof(tracy::VkCtxScope));

    new(&zoneGpu.Impl) tracy::VkCtxScope(
        (TracyVkCtx)ProfilerContext::Get()->GraphicsContext(),
        (const tracy::SourceLocationData*)&sourceLocationData,
        DeviceInternal::GetProfilerCommandBuffer(ProfilerContext::Get()), true);
}

void Device::DestroyGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu)
{
    std::launder((tracy::VkCtxScope*)&zoneGpu.Impl)->~VkCtxScope();
}

void Device::CollectGpuProfileFrames()
{
    TracyVkCollect(
        ((TracyVkCtx)ProfilerContext::Get()->GraphicsContext()),
        DeviceInternal::GetProfilerCommandBuffer(ProfilerContext::Get()))
}

ImTextureID Device::CreateImGuiImage(const ImageSubresource& texture, Sampler sampler, ImageLayout layout)
{
    const ImageViewHandle viewHandle = GetImageViewHandle(texture.Image, texture.Description);
    const VkDescriptorSet imageDescriptorSet = ImGui_ImplVulkan_AddTexture(deviceResources().NativeHandleToVk(sampler),
        deviceResources().Hot(texture.Image).Views.ViewList[viewHandle.m_Index],
        vulkanImageLayoutFromImageLayout(layout));

    return ImTextureID{imageDescriptorSet};
}

void Device::DestroyImGuiImage(ImTextureID image)
{
    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)image);
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
#ifdef VULKAN_VAL_LAYERS
    VkDebugUtilsLabelEXT debugLabel = {};
    debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debugLabel.pLabelName = label.data();
    vkCmdBeginDebugUtilsLabelEXT(deviceResources().NativeHandleToVk(cmd), &debugLabel);
#endif // VULKAN_VAL_LAYERS
}

void Device::EndCommandBufferLabel(CommandBuffer cmd)
{
#ifdef VULKAN_VAL_LAYERS
    vkCmdEndDebugUtilsLabelEXT(deviceResources().NativeHandleToVk(cmd));
#endif // VULKAN_VAL_LAYERS
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
    nameVulkanHandle(g_State.Device, (u64)deviceResources().Get(buffer).Buffer, VK_OBJECT_TYPE_BUFFER, name);
}

void Device::NameImage(Image image, std::string_view name)
{
    nameVulkanHandle(g_State.Device, (u64)deviceResources().Hot(image).Image, VK_OBJECT_TYPE_IMAGE, name);
}

void Device::NamePipeline(Pipeline pipeline, std::string_view name)
{
    nameVulkanHandle(g_State.Device, (u64)deviceResources().NativeHandleToVk(pipeline), VK_OBJECT_TYPE_PIPELINE, name);
}

void Device::CompileCommand(CommandBuffer cmd, ExecuteSecondaryBufferCommand&& command)
{
    VkCommandBuffer secondary = deviceResources().NativeHandleToVk(command.Cmd);
    vkCmdExecuteCommands(deviceResources().NativeHandleToVk(cmd), 1, &secondary);
}

void Device::CompileCommand(CommandBuffer cmd, PrepareSwapchainPresentCommand&& command)
{
    SwapchainResource::ColdData cold = deviceResources().Cold(command.Swapchain);

    ImageSubresource drawSubresource = {
        .Image = cold.Description.DrawImage,
        .Description = {.Mipmaps = 1, .Layers = 1}
    };
    ImageSubresource presentSubresource = {
        .Image = cold.Description.ColorImages[command.ImageIndex],
        .Description = {.Mipmaps = 1, .Layers = 1}
    };

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

    CompileCommand(cmd, WaitOnBarrierCommand{
        .DependencyInfo = presentToDestinationTransitionInfo
    });

    ImageSubregion sourceSubregion = {
        .Mipmap = (u32)drawSubresource.Description.MipmapBase,
        .LayerBase = (u32)drawSubresource.Description.LayerBase,
        .Layers = (u32)drawSubresource.Description.Layers,
        .Top = GetImageDescription(cold.Description.DrawImage).Dimensions()
    };

    ImageSubregion destinationSubregion = {
        .Mipmap = (u32)presentSubresource.Description.MipmapBase,
        .LayerBase = (u32)presentSubresource.Description.LayerBase,
        .Layers = (u32)presentSubresource.Description.Layers,
        .Top = GetImageDescription(cold.Description.ColorImages[command.ImageIndex]).
        Dimensions()
    };

    CompileCommand(cmd, BlitImageCommand{
        .Source = cold.Description.DrawImage,
        .Destination = cold.Description.ColorImages[command.ImageIndex],
        .Filter = ImageFilter::Linear,
        .SourceSubregion = sourceSubregion,
        .DestinationSubregion = destinationSubregion
    });

    CompileCommand(cmd, WaitOnBarrierCommand{
        .DependencyInfo = destinationToPresentTransitionInfo
    });
}

void Device::CompileCommand(CommandBuffer cmd, BeginRenderingCommand&& command)
{
    auto& renderingInfo = command.RenderingInfo;
    
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(renderingInfo.ColorAttachments.size());
    for (auto& color : renderingInfo.ColorAttachments)
        colorAttachments.emplace_back(DeviceInternal::CreateVulkanRenderingAttachmentInfo(color));
    std::optional<VkRenderingAttachmentInfo> depthStencilAttachment = std::nullopt;
    if (renderingInfo.DepthAttachment.has_value())
        depthStencilAttachment = DeviceInternal::CreateVulkanRenderingAttachmentInfo(*renderingInfo.DepthAttachment);
    
    VkRenderingInfo renderingInfoVulkan = {};
    renderingInfoVulkan.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfoVulkan.layerCount = 1;
    renderingInfoVulkan.renderArea = VkRect2D{
        .offset = {},
        .extent = {renderingInfo.RenderArea.x, renderingInfo.RenderArea.y}
    };
    renderingInfoVulkan.colorAttachmentCount = (u32)colorAttachments.size();
    renderingInfoVulkan.pColorAttachments = colorAttachments.data();
    if (depthStencilAttachment.has_value())
        renderingInfoVulkan.pDepthAttachment = &depthStencilAttachment.value();

    vkCmdBeginRendering(deviceResources().NativeHandleToVk(cmd), &renderingInfoVulkan);
}

void Device::CompileCommand(CommandBuffer cmd, EndRenderingCommand&& command)
{
    vkCmdEndRendering(deviceResources().NativeHandleToVk(cmd));
}

void Device::CompileCommand(CommandBuffer, ImGuiBeginCommand&&)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Device::CompileCommand(CommandBuffer cmd, ImGuiEndCommand&& command)
{
    ImGui::Render();
    CompileCommand(cmd, BeginRenderingCommand{
        .RenderingInfo = std::move(command.RenderingInfo)
    });
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), deviceResources().NativeHandleToVk(cmd));
    CompileCommand(cmd, EndRenderingCommand{});
}

void Device::CompileCommand(CommandBuffer cmd, BeginConditionalRenderingCommand&& command)
{
    VkConditionalRenderingBeginInfoEXT beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.buffer = deviceResources().Get(command.Buffer).Buffer;
    beginInfo.offset = command.Offset;

    vkCmdBeginConditionalRenderingEXT(deviceResources().NativeHandleToVk(cmd), &beginInfo);
}

void Device::CompileCommand(CommandBuffer cmd, EndConditionalRenderingCommand&&)
{
    vkCmdEndConditionalRenderingEXT(deviceResources().NativeHandleToVk(cmd));
}

void Device::CompileCommand(CommandBuffer cmd, SetViewportCommand&& command)
{
    const VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)command.Size.x, .height = (f32)command.Size.y,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };

    vkCmdSetViewport(deviceResources().NativeHandleToVk(cmd), 0, 1, &viewport);
}

void Device::CompileCommand(CommandBuffer cmd, SetScissorsCommand&& command)
{
    const VkRect2D scissor = {
        .offset = {(i32)command.Offset.x, (i32)command.Offset.y},
        .extent = {(u32)command.Size.x, (u32)command.Size.y}
    };

    vkCmdSetScissor(deviceResources().NativeHandleToVk(cmd), 0, 1, &scissor);
}

void Device::CompileCommand(CommandBuffer cmd, SetDepthBiasCommand&& command)
{
    vkCmdSetDepthBias(deviceResources().NativeHandleToVk(cmd), command.Constant, 0.0f, command.Slope);
}

void Device::CompileCommand(CommandBuffer cmd, CopyBufferCommand&& command)
{
    VkBufferCopy2 copy = {};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy.size = command.SizeBytes;
    copy.srcOffset = command.SourceOffset;
    copy.dstOffset = command.DestinationOffset;

    VkCopyBufferInfo2 copyBufferInfo = {};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = deviceResources().Get(command.Source).Buffer;
    copyBufferInfo.dstBuffer = deviceResources().Get(command.Destination).Buffer;
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copy;

    vkCmdCopyBuffer2(deviceResources().NativeHandleToVk(cmd), &copyBufferInfo);
}

void Device::CompileCommand(CommandBuffer cmd, CopyBufferToImageCommand&& command)
{
    ASSERT(command.ImageSubresource.Mipmaps == 1, "Buffer to image copies one mipmap at a time")

    const ImageResource::HotData& image = deviceResources().Hot(command.Image);

    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.bufferOffset = command.BufferOffset;
    bufferImageCopy.imageExtent = {
        .width = std::max(1u, image.Width >> command.ImageSubresource.MipmapBase),
        .height = std::max(1u, image.Height >> command.ImageSubresource.MipmapBase),
        .depth = image.Kind == ImageKind::Image3d ? image.LayersDepth >> command.ImageSubresource.MipmapBase : 1u
    };
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(image.Usage);
    bufferImageCopy.imageSubresource.mipLevel = (u32)(i32)command.ImageSubresource.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = (u32)(i32)command.ImageSubresource.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = (u32)(i32)command.ImageSubresource.Layers;

    VkCopyBufferToImageInfo2 copyBufferToImageInfo = {};
    copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copyBufferToImageInfo.srcBuffer = deviceResources().Get(command.Buffer).Buffer;
    copyBufferToImageInfo.dstImage = image.Image;
    copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyBufferToImageInfo.regionCount = 1;
    copyBufferToImageInfo.pRegions = &bufferImageCopy;

    vkCmdCopyBufferToImage2(deviceResources().NativeHandleToVk(cmd), &copyBufferToImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, CopyImageCommand&& command)
{
    const ImageResource::HotData source = deviceResources().Hot(command.Source);
    const ImageResource::HotData destination = deviceResources().Hot(command.Destination);

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
    imageCopy.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(source.Usage);
    imageCopy.srcSubresource.baseArrayLayer = command.SourceSubregion.LayerBase;
    imageCopy.srcSubresource.layerCount = command.SourceSubregion.Layers;
    imageCopy.srcSubresource.mipLevel = command.SourceSubregion.Mipmap;
    imageCopy.srcOffset = VkOffset3D{
        .x = (i32)command.SourceSubregion.Bottom.x,
        .y = (i32)command.SourceSubregion.Bottom.y,
        .z = (i32)command.SourceSubregion.Bottom.z
    };
    imageCopy.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destination.Usage);
    imageCopy.dstSubresource.baseArrayLayer = command.DestinationSubregion.LayerBase;
    imageCopy.dstSubresource.layerCount = command.DestinationSubregion.Layers;
    imageCopy.dstSubresource.mipLevel = command.DestinationSubregion.Mipmap;
    imageCopy.dstOffset = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Bottom.x,
        .y = (i32)command.DestinationSubregion.Bottom.y,
        .z = (i32)command.DestinationSubregion.Bottom.z
    };

    copyImageInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
    copyImageInfo.srcImage = source.Image;
    copyImageInfo.dstImage = destination.Image;
    copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImageInfo.regionCount = 1;

    copyImageInfo.pRegions = &imageCopy;

    vkCmdCopyImage2(deviceResources().NativeHandleToVk(cmd), &copyImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, BlitImageCommand&& command)
{
    const ImageResource::HotData& source = deviceResources().Hot(command.Source);
    const ImageResource::HotData& destination = deviceResources().Hot(command.Destination);

    VkImageBlit2 imageBlit = {};
    VkBlitImageInfo2 blitImageInfo = {};

    imageBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    imageBlit.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(source.Usage);
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

    imageBlit.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destination.Usage);
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
    blitImageInfo.srcImage = source.Image;
    blitImageInfo.dstImage = destination.Image;
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.filter = vulkanFilterFromImageFilter(command.Filter);

    blitImageInfo.pRegions = &imageBlit;

    vkCmdBlitImage2(deviceResources().NativeHandleToVk(cmd), &blitImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, MipmapImageCommand&& command)
{
    const ImageResource::HotData& image = deviceResources().Hot(command.Image);

    i32 width = (i32)image.Width;
    i32 height = (i32)image.Height;
    i32 depth = image.Kind == ImageKind::Image3d ? (i32)image.LayersDepth : 1;
    i8 layers = image.Kind != ImageKind::Image3d ? (i8)image.LayersDepth : (i8)1;

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

    CompileCommand(cmd, WaitOnBarrierCommand{
        .DependencyInfo = transitionInfo
    });

    for (i8 mip = 1; mip < image.Mipmaps; mip++)
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
        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = transitionInfo
        });

        CompileCommand(cmd, BlitImageCommand{
            .Source = command.Image,
            .Destination = command.Image,
            .Filter = image.MipmapFilter,
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

        CompileCommand(cmd, WaitOnBarrierCommand{
            .DependencyInfo = transitionInfo
        });
    }
}

void Device::CompileCommand(CommandBuffer cmd, ClearColorImageCommand&& command)
{
    const VkClearColorValue clearValue = {
        .float32 = {
            command.ClearValue.F.r,
            command.ClearValue.F.g,
            command.ClearValue.F.b,
            command.ClearValue.F.a
        }
    };
    const VkImageSubresourceRange imageSubresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = (u32)command.Image.Description.MipmapBase,
        .levelCount = (u32)command.Image.Description.Mipmaps,
        .baseArrayLayer = (u32)command.Image.Description.LayerBase,
        .layerCount = (u32)command.Image.Description.Layers, 
    };
    vkCmdClearColorImage(deviceResources().NativeHandleToVk(cmd), deviceResources().Hot(command.Image.Image).Image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &imageSubresourceRange);
}

void Device::CompileCommand(CommandBuffer cmd, ClearDepthStencilImageCommand&& command)
{
    const VkClearDepthStencilValue clearValue = {
        .depth = command.ClearValue.Depth,
        .stencil = command.ClearValue.Stencil
    };
    const VkImageSubresourceRange imageSubresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .baseMipLevel = (u32)command.Image.Description.MipmapBase,
        .levelCount = (u32)command.Image.Description.Mipmaps,
        .baseArrayLayer = (u32)command.Image.Description.LayerBase,
        .layerCount = (u32)command.Image.Description.Layers,
    };
    vkCmdClearDepthStencilImage(deviceResources().NativeHandleToVk(cmd),
        deviceResources().Hot(command.Image.Image).Image, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &imageSubresourceRange);
}

void Device::CompileCommand(CommandBuffer cmd, WaitOnFullPipelineBarrierCommand&& command)
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

    vkCmdPipelineBarrier2(deviceResources().NativeHandleToVk(cmd), &dependencyInfo);
}

namespace 
{
void barrierCommand(const auto& command, const auto& vkCall)
{
    const DependencyInfo& dependencyInfo = command.DependencyInfo;

    VkDependencyInfo vkDependencyInfo = {};
    vkDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    
    VkMemoryBarrier2 memoryBarrier = {};
    VkImageMemoryBarrier2 imageMemoryBarrier = {};
    
    switch (dependencyInfo.Type) 
    {
    case DependencyInfoType::Execution:
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
            dependencyInfo.Dependency.Execution.SourceStage);
        memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
            dependencyInfo.Dependency.Execution.DestinationStage);
        vkDependencyInfo.memoryBarrierCount = 1;
        vkDependencyInfo.pMemoryBarriers = &memoryBarrier;
        break;
    case DependencyInfoType::Memory:
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
            dependencyInfo.Dependency.Memory.SourceStage);
        memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
            dependencyInfo.Dependency.Memory.DestinationStage);
        memoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(
            dependencyInfo.Dependency.Memory.SourceAccess);
        memoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(
            dependencyInfo.Dependency.Memory.DestinationAccess);
        vkDependencyInfo.memoryBarrierCount = 1;
        vkDependencyInfo.pMemoryBarriers = &memoryBarrier;
        break;
    case DependencyInfoType::Layout:
        {
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageMemoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(
                dependencyInfo.Dependency.Layout.SourceStage);
            imageMemoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(
                dependencyInfo.Dependency.Layout.DestinationStage);
            imageMemoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(
                dependencyInfo.Dependency.Layout.SourceAccess);
            imageMemoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(
                dependencyInfo.Dependency.Layout.DestinationAccess);
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.oldLayout = vulkanImageLayoutFromImageLayout(dependencyInfo.Dependency.Layout.OldLayout);
            imageMemoryBarrier.newLayout = vulkanImageLayoutFromImageLayout(dependencyInfo.Dependency.Layout.NewLayout);
            const ImageResource::HotData& hot = deviceResources().Hot(
                dependencyInfo.Dependency.Layout.ImageSubresource.Image);
            imageMemoryBarrier.image = hot.Image;
            imageMemoryBarrier.subresourceRange = {
                .aspectMask = vulkanImageAspectFromImageUsage(hot.Usage),
                .baseMipLevel = (u32)dependencyInfo.Dependency.Layout.ImageSubresource.Description.MipmapBase,
                .levelCount = (u32)dependencyInfo.Dependency.Layout.ImageSubresource.Description.Mipmaps,
                .baseArrayLayer = (u32)dependencyInfo.Dependency.Layout.ImageSubresource.Description.LayerBase,
                .layerCount = (u32)dependencyInfo.Dependency.Layout.ImageSubresource.Description.Layers
            };
            vkDependencyInfo.imageMemoryBarrierCount = 1;
            vkDependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
        }
        break;
    default:
        ASSERT(false, "Unexpected dependency type")
        break;
    }

    vkCall(vkDependencyInfo);
}
}

void Device::CompileCommand(CommandBuffer cmd, WaitOnBarrierCommand&& command)
{
    barrierCommand(command, [&](const VkDependencyInfo& dependencyInfo) {
        vkCmdPipelineBarrier2(deviceResources().NativeHandleToVk(cmd), &dependencyInfo);
    });
}

void Device::CompileCommand(CommandBuffer cmd, SignalSplitBarrierCommand&& command)
{
    barrierCommand(command, [&](const VkDependencyInfo& dependencyInfo) {
        vkCmdSetEvent2(deviceResources().NativeHandleToVk(cmd), 
            deviceResources().NativeHandleToVk(command.SplitBarrier), &dependencyInfo);
    });
}

void Device::CompileCommand(CommandBuffer cmd, WaitOnSplitBarrierCommand&& command)
{
    barrierCommand(command, [&](const VkDependencyInfo& dependencyInfo) {
        VkEvent event = deviceResources().NativeHandleToVk(command.SplitBarrier);
        vkCmdWaitEvents2(deviceResources().NativeHandleToVk(cmd), 1, &event, &dependencyInfo);
    });
}

void Device::CompileCommand(CommandBuffer cmd, ResetSplitBarrierCommand&& command)
{
    vkCmdResetEvent2(deviceResources().NativeHandleToVk(cmd), deviceResources().NativeHandleToVk(command.SplitBarrier),
        vulkanPipelineStageFromPipelineStage(command.DestinationStage));
}

void Device::CompileCommand(CommandBuffer cmd, BindVertexBuffersCommand&& command)
{
    std::vector<VkBuffer> vkBuffers(command.Buffers.size());
    for (u32 i = 0; i < vkBuffers.size(); i++)
        vkBuffers[i] = deviceResources().Get(command.Buffers[i]).Buffer;

    vkCmdBindVertexBuffers(deviceResources().NativeHandleToVk(cmd), 0, (u32)vkBuffers.size(), vkBuffers.data(),
        command.Offsets.data());
}

void Device::CompileCommand(CommandBuffer cmd, BindIndexU32BufferCommand&& command)
{
    vkCmdBindIndexBuffer(deviceResources().NativeHandleToVk(cmd), deviceResources().Get(command.Buffer).Buffer, 
        command.Offset, VK_INDEX_TYPE_UINT32);
}

void Device::CompileCommand(CommandBuffer cmd, BindIndexU16BufferCommand&& command)
{
    vkCmdBindIndexBuffer(deviceResources().NativeHandleToVk(cmd), deviceResources().Get(command.Buffer).Buffer, 
        command.Offset, VK_INDEX_TYPE_UINT16);
}

void Device::CompileCommand(CommandBuffer cmd, BindIndexU8BufferCommand&& command)
{
    vkCmdBindIndexBuffer(deviceResources().NativeHandleToVk(cmd), deviceResources().Get(command.Buffer).Buffer,
        command.Offset, VK_INDEX_TYPE_UINT8_EXT);
}

void Device::CompileCommand(CommandBuffer cmd, BindPipelineGraphicsCommand&& command)
{
    vkCmdBindPipeline(deviceResources().NativeHandleToVk(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
        deviceResources().NativeHandleToVk(command.Pipeline));
}

void Device::CompileCommand(CommandBuffer cmd, BindPipelineComputeCommand&& command)
{
    vkCmdBindPipeline(deviceResources().NativeHandleToVk(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
        deviceResources().NativeHandleToVk(command.Pipeline));
}

#ifdef DESCRIPTOR_BUFFER
void Device::CompileCommand(CommandBuffer cmd, BindDescriptorArenaAllocatorsCommand&& command)
{
    std::vector<VkDescriptorBufferBindingInfoEXT> descriptorBufferBindings;
    descriptorBufferBindings.reserve(command.Allocators->m_TransientAllocators.size());

    for (auto& allocator : command.Allocators->m_TransientAllocators)
    {
        DescriptorArenaAllocatorResource::HotData& hot = deviceResources().Hot(allocator);
        const u64 deviceAddress = hot.DeviceAddress;
        VkDescriptorBufferBindingInfoEXT binding = {};
        binding.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        binding.address = deviceAddress;
        binding.usage = hot.DescriptorBufferUsage;

        descriptorBufferBindings.push_back(binding);
    }

    vkCmdBindDescriptorBuffersEXT(deviceResources().NativeHandleToVk(cmd), (u32)descriptorBufferBindings.size(),
        descriptorBufferBindings.data());
}
void Device::CompileCommand(CommandBuffer cmd, BindImmutableSamplersGraphicsCommand&& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(deviceResources().NativeHandleToVk(cmd),
        VK_PIPELINE_BIND_POINT_GRAPHICS, deviceResources().Hot(command.PipelineLayout).Layout, command.Set);
}

void Device::CompileCommand(CommandBuffer cmd, BindImmutableSamplersComputeCommand&& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(deviceResources().NativeHandleToVk(cmd),
        VK_PIPELINE_BIND_POINT_COMPUTE, deviceResources().Hot(command.PipelineLayout).Layout, command.Set);
}
#else // DESCRIPTOR_BUFFER
void Device::CompileCommand(CommandBuffer cmd, BindImmutableSamplersGraphicsCommand&& command)
{
    VkDescriptorSet vkDescriptors = deviceResources().NativeHandleToVk(command.Descriptors);
    vkCmdBindDescriptorSets(deviceResources().NativeHandleToVk(cmd),
        VK_PIPELINE_BIND_POINT_GRAPHICS, deviceResources().Hot(command.PipelineLayout).Layout, command.Set, 1,
        &vkDescriptors, 0, nullptr);
}

void Device::CompileCommand(CommandBuffer cmd, BindImmutableSamplersComputeCommand&& command)
{
    VkDescriptorSet vkDescriptors = deviceResources().NativeHandleToVk(command.Descriptors);
    vkCmdBindDescriptorSets(deviceResources().NativeHandleToVk(cmd),
        VK_PIPELINE_BIND_POINT_COMPUTE, deviceResources().Hot(command.PipelineLayout).Layout, command.Set, 1,
        &vkDescriptors, 0, nullptr);
}

void Device::CompileCommand(CommandBuffer, BindDescriptorArenaAllocatorsCommand&&)
{
}
#endif // DESCRIPTOR_BUFFER

void Device::CompileCommand(CommandBuffer cmd, BindDescriptorsGraphicsCommand&& command)
{
    DeviceInternal::BindDescriptors(cmd, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void Device::CompileCommand(CommandBuffer cmd, BindDescriptorsComputeCommand&& command)
{
    DeviceInternal::BindDescriptors(cmd, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_COMPUTE);
}

void Device::CompileCommand(CommandBuffer cmd, PushConstantsCommand&& command)
{
    auto pushConstants = deviceResources().Cold(command.PipelineLayout).PushConstants;
    const VkPushConstantRange& pushConstantRange = pushConstants.front();
        vkCmdPushConstants(deviceResources().NativeHandleToVk(cmd), 
        deviceResources().Hot(command.PipelineLayout).Layout,
        pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, command.Data.data());
}

void Device::CompileCommand(CommandBuffer cmd, DrawCommand&& command)
{
    vkCmdDraw(deviceResources().NativeHandleToVk(cmd), command.VertexCount, 1, 0, command.BaseInstance);
}

void Device::CompileCommand(CommandBuffer cmd, DrawIndexedCommand&& command)
{
    vkCmdDrawIndexed(deviceResources().NativeHandleToVk(cmd), command.IndexCount, 1, 0, 0, command.BaseInstance);
}

void Device::CompileCommand(CommandBuffer cmd, DrawIndexedIndirectCommand&& command)
{
    vkCmdDrawIndexedIndirect(deviceResources().NativeHandleToVk(cmd), deviceResources().Get(command.Buffer).Buffer,
        command.Offset, command.Count, command.Stride);
}

void Device::CompileCommand(CommandBuffer cmd, DrawIndexedIndirectCountCommand&& command)
{
    vkCmdDrawIndexedIndirectCount(deviceResources().NativeHandleToVk(cmd),
        deviceResources().Get(command.DrawBuffer).Buffer, command.DrawOffset,
        deviceResources().Get(command.CountBuffer).Buffer, command.CountOffset,
        command.MaxCount, command.Stride);
}

void Device::CompileCommand(CommandBuffer cmd, DispatchCommand&& command)
{
    const glm::uvec3 groupSize = (command.Invocations + command.GroupSize - glm::uvec3{1}) / command.GroupSize;
    vkCmdDispatch(deviceResources().NativeHandleToVk(cmd), groupSize.x, groupSize.y, groupSize.z);
}

void Device::CompileCommand(CommandBuffer cmd, DispatchIndirectCommand&& command)
{
    vkCmdDispatchIndirect(deviceResources().NativeHandleToVk(cmd), deviceResources().Get(command.Buffer).Buffer, 
        command.Offset);
}


VmaAllocator& DeviceInternal::Allocator()
{
    return g_State.Allocator;
}

VkCommandBuffer DeviceInternal::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return deviceResources().NativeHandleToVk(context->m_GraphicsCommandBuffers[context->m_CurrentFrame]);
}

Buffer DeviceInternal::AllocateBuffer(const BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags allocationFlags)
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
        &bufferResource.Buffer, &bufferResource.Allocation, nullptr), "Failed to create a buffer");

    bufferResource.Description.SizeBytes = createInfo.Description.SizeBytes;
    bufferResource.Description.Usage = createInfo.Description.Usage;
    if (createInfo.PersistentMapping)
        bufferResource.HostAddress = bufferResource.Allocation->GetMappedData();
    
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = bufferResource.Buffer;
    bufferResource.DeviceAddress = vkGetBufferDeviceAddress(g_State.Device, &deviceAddressInfo);

    return {deviceResources().Add(std::move(bufferResource))};
}

VkImageView DeviceInternal::CreateVulkanImageView(ImageKind kind, ImageUsage usage,
    const ImageSubresourceDescription& subresourceDescription, VkImage image, VkFormat format)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.format = format;
    createInfo.viewType = vulkanImageViewTypeFromImageAndViewKind(kind, subresourceDescription.ImageViewKind);
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = vulkanImageAspectFromImageUsage(usage);
    createInfo.subresourceRange.baseMipLevel = (u32)(i32)subresourceDescription.MipmapBase;
    createInfo.subresourceRange.levelCount = (u32)(i32)subresourceDescription.Mipmaps;
    createInfo.subresourceRange.baseArrayLayer = (u32)(i32)subresourceDescription.LayerBase;
    createInfo.subresourceRange.layerCount = (u32)(i32)subresourceDescription.Layers;

    VkImageView imageView;

    deviceCheck(vkCreateImageView(g_State.Device, &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

VkRenderingAttachmentInfo DeviceInternal::CreateVulkanRenderingAttachmentInfo(const RenderingAttachment& attachment)
{
    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.clearValue = VkClearValue{
        .color = {
            .float32 = {
                attachment.Description.Clear.Color.F.r,
                attachment.Description.Clear.Color.F.g,
                attachment.Description.Clear.Color.F.b,
                attachment.Description.Clear.Color.F.a
            }
        }
    };
    attachmentInfo.imageLayout = vulkanImageLayoutFromImageLayout(attachment.Layout);
    attachmentInfo.imageView = deviceResources().Hot(attachment.Image).Views.ViewList[
        Device::GetImageViewHandle(attachment.Image, attachment.Description.Subresource).m_Index];
    attachmentInfo.loadOp = vulkanAttachmentLoadFromAttachmentLoad(attachment.Description.OnLoad);
    attachmentInfo.storeOp = vulkanAttachmentStoreFromAttachmentStore(attachment.Description.OnStore);
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    
    return attachmentInfo;
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

void DeviceInternal::WriteDescriptor(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    u32 index, VkDescriptorGetInfoEXT& descriptorGetInfo)
{
    auto&& [slot, type] = slotInfo;
    
    DescriptorsResource::ColdData& cold = deviceResources().Cold(descriptors);
    const u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    const u64 innerOffsetBytes = descriptorSizeBytes * index;
    ASSERT(innerOffsetBytes + descriptorSizeBytes <= cold.SizeBytes,
        "Trying to write descriptor outside of the allocated region")

    const u64 offsetBytes = cold.Offsets[slot] + innerOffsetBytes;
    vkGetDescriptorEXT(g_State.Device, &descriptorGetInfo, descriptorSizeBytes,
        (u8*)deviceResources().Hot(cold.Allocator).MappedAddress + offsetBytes);
}

void DeviceInternal::BindDescriptors(CommandBuffer cmd, PipelineLayout pipelineLayout,
    Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    DescriptorsResource::ColdData& cold = deviceResources().Cold(descriptors);
    const u64 offset = cold.Offsets.front();
    u32 set = deviceResources().Hot(cold.Allocator).DescriptorSet;
    vkCmdSetDescriptorBufferOffsetsEXT(deviceResources().NativeHandleToVk(cmd), bindPoint,
        deviceResources().Hot(pipelineLayout).Layout, firstSet, 1, &set, &offset);
}
#else

u32 DeviceInternal::GetFreePoolIndexFromAllocator(DescriptorArenaAllocator allocator, DescriptorPoolFlags poolFlags)
{
    DescriptorArenaAllocatorResource::ColdData& coldData = deviceResources().Cold(allocator);
    for (u32 i = 0; i < coldData.FreePools.size(); i++)
        if (coldData.FreePools[i].Flags == poolFlags)
            return i;

    const u32 index = (u32)coldData.FreePools.size();
    std::vector<VkDescriptorPoolSize> sizes(DescriptorArenaAllocatorResource::ColdData::POOL_SIZES.size());
    for (u32 i = 0; i < sizes.size(); i++)
        sizes[i] = {
            .type = vulkanDescriptorTypeFromDescriptorType(
                DescriptorArenaAllocatorResource::ColdData::POOL_SIZES[i].DescriptorType),
            .descriptorCount =
                (u32)(DescriptorArenaAllocatorResource::ColdData::POOL_SIZES[i].SetSizeMultiplier * 
                    (f32)coldData.MaxSetsPerPool)
        };

    VkDescriptorPool pool = {};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = coldData.MaxSetsPerPool;
    poolCreateInfo.poolSizeCount = (u32)sizes.size();
    poolCreateInfo.pPoolSizes = sizes.data();
    poolCreateInfo.flags = vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(poolFlags);

    deviceCheck(vkCreateDescriptorPool(g_State.Device, &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    coldData.FreePools.push_back({.Pool = pool, .Flags = poolFlags});

    return index;
}

void DeviceInternal::BindDescriptors(CommandBuffer cmd, PipelineLayout pipelineLayout,
    Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    VkDescriptorSet vkDescriptors = deviceResources().NativeHandleToVk(descriptors);
    vkCmdBindDescriptorSets(deviceResources().NativeHandleToVk(cmd), bindPoint,
        deviceResources().Hot(pipelineLayout).Layout, firstSet, 1, &vkDescriptors, 0, nullptr);
}
#endif

std::vector<VkSemaphoreSubmitInfo> DeviceInternal::CreateVulkanSemaphoreSubmit(Span<const Semaphore> semaphores, 
    Span<const PipelineStage> waitStages)
{
    std::vector waitSemaphoreSubmitInfos(semaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(semaphores))
    {
        waitSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreSubmitInfos[i].semaphore = deviceResources().NativeHandleToVk(semaphore);
        waitSemaphoreSubmitInfos[i].stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i]);
    }

    return waitSemaphoreSubmitInfos;
}

std::vector<VkSemaphoreSubmitInfo> DeviceInternal::CreateVulkanSemaphoreSubmit(Span<const TimelineSemaphore> semaphores, 
    Span<const u64> waitValues, Span<const PipelineStage> waitStages)
{
    std::vector waitSemaphoreSubmitInfos(semaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(semaphores))
    {
        waitSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreSubmitInfos[i].semaphore = deviceResources().NativeHandleToVk(semaphore);
        waitSemaphoreSubmitInfos[i].value = waitValues[i];
        waitSemaphoreSubmitInfos[i].stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i]);
    }

    return waitSemaphoreSubmitInfos;
}
