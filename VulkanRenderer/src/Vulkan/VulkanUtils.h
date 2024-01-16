#pragma once
#include "VulkanCommon.h"
#include "VulkanCore.h"
#include "Driver.h"

#include <volk.h>

#include "TextureAsset.h"

namespace vkUtils
{
    inline SurfaceDetails getSurfaceDetails(VkPhysicalDevice gpu, VkSurfaceKHR surface)
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

    inline VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipmapLevels, u32 baseMipLevel = 0)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.format = format;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = aspectFlags;
        createInfo.subresourceRange.baseMipLevel = baseMipLevel;
        createInfo.subresourceRange.levelCount = mipmapLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;

        VulkanCheck(vkCreateImageView(device, &createInfo, nullptr, &imageView), "Failed to create image view");

        return imageView;
    }

    inline ShaderKind shaderKindByStage(VkShaderStageFlags stage)
    {
        switch ((VkShaderStageFlagBits)stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return ShaderKind::Vertex;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return ShaderKind::Pixel;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return ShaderKind::Compute;
        default:
            ASSERT(false, "Unsopported shader kind")
        }
        std::unreachable();
    }

    inline VkBufferUsageFlags vkBufferUsageByKind(BufferKind kind)
    {
        switch (kind.Kind)
        {
        case BufferKind::Vertex:        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferKind::Index:         return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferKind::Uniform:       return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case BufferKind::Storage:       return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferKind::Indirect:      return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        case BufferKind::Source:        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferKind::Destination:   return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferKind::Conditional:   return VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
        case BufferKind::None:
            ASSERT(false, "Buffer kind is unset")
            break;
        default:
            ASSERT(false, "Unrecognized buffer kind")
            break;
        }
        std::unreachable();
    }

    inline VkPrimitiveTopology vkTopologyByPrimitiveKind(PrimitiveKind kind)
    {
        switch (kind) {
        case PrimitiveKind::Triangle: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveKind::Point:    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default:
            ASSERT(false, "Unrecognized primitive kind")
            break;
        }
        std::unreachable();
    }

    inline u64 alignUniformBufferSizeBytes(u64 sizeBytes)
    {
        u64 alignment = Driver::GetUniformBufferAlignment();
        u64 mask = alignment - 1;
        if (alignment != 0) // intel gpu has 0 alignment
            return (sizeBytes + mask) & ~mask;
        return sizeBytes;
    }

    template <typename T>
    u64 alignUniformBufferSizeBytes(u32 frames)
    {
        u64 alignment = Driver::GetUniformBufferAlignment();
        u64 mask = alignment - 1;
        u64 sizeBytes = sizeof(T);
        if (alignment != 0) // intel gpu has 0 alignment
            sizeBytes = (sizeBytes + mask) & ~mask;
        return sizeBytes * frames;
    }
    
    inline u32 formatSizeBytes(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R4G4_UNORM_PACK8:
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
            return 1;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:
            return 2;
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:
            return 3;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            return 4;
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:
            return 6;
        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64_SFLOAT:
            return 8;
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 12;
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64_SFLOAT:
            return 16;
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64_SFLOAT:
            return 24;
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            return 32;
        default:
            ASSERT(false, "Not scientifically possible")
        }
        std::unreachable();
    }

    inline VkSamplerMipmapMode mipmapModeFromSamplerFilter(VkFilter filter)
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

    inline VkFormat vkFormatByTextureAssetFormat(assetLib::TextureFormat format)
    {
        switch (format)
        {
        case assetLib::TextureFormat::Unknown:
            return VK_FORMAT_UNDEFINED;
        case assetLib::TextureFormat::SRGBA8:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case assetLib::TextureFormat::RGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            ASSERT(false, "Unsupported asset texture format")
        }
        std::unreachable();
    }
}
