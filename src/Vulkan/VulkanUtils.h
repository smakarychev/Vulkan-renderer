#pragma once
#include "VulkanCommon.h"

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

    inline VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipmapLevels)
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
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = mipmapLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;

        VulkanCheck(vkCreateImageView(device, &createInfo, nullptr, &imageView), "Failed to create image view");

        return imageView;
    }
}
