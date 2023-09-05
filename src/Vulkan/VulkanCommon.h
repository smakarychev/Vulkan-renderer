#pragma once

#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class RenderCommand;

enum class QueueKind {Graphics, Presentation};

struct QueueInfo
{
    // technically any family index is possible;
    // practically GPUs have only a few (usually less than 5)
    static constexpr u32 UNSET_FAMILY = std::numeric_limits<u32>::max();
    VkQueue Queue{VK_NULL_HANDLE};
    u32 Family{UNSET_FAMILY};  
};

struct SurfaceDetails
{
public:
    bool IsSufficient() const
    {
        return !(Formats.empty() || PresentModes.empty());
    }
public:
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
};

// todo:
// placeholder for actual texture class
struct ImageData
{
    VkImage Image;
    VkImageView View;
    u32 Width;
    u32 Height;
};