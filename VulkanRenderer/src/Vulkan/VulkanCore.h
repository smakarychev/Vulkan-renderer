#pragma once

#include <vulkan/vulkan_core.h>
#include <string_view>

#include "Core/core.h"

inline void VulkanCheck(VkResult res, std::string_view message)
{
    if (res != VK_SUCCESS)
    {
        LOG(message.data());
        abort();
    }
}
