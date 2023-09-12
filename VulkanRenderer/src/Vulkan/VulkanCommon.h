#pragma once

#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "core.h"

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class RenderCommand;

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

enum class QueueKind {Graphics, Presentation};
struct BufferKind
{
    enum Flags
    {
        None = 0,
        Vertex = BIT(1),
        Index = BIT(2),
        Uniform = BIT(3),
        Storage = BIT(4),
        Indirect = BIT(5),
        Source = BIT(6),
        Destination = BIT(7)
    };
    BufferKind() = default;
    BufferKind(Flags kind) : Kind(kind) {}
    Flags Kind{None};
};

struct QueueInfo
{
    // technically any family index is possible;
    // practically GPUs have only a few
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

struct ImageData
{
    VkImage Image;
    VkImageView View;
    u32 Width;
    u32 Height;
};