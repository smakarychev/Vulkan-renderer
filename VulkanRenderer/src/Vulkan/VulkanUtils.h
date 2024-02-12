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
}
