#pragma once

#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "ImageTraits.h"
#include "RenderHandle.h"
#include "RenderObject.h"
#include "Core/core.h"

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class RenderCommand;

class Buffer;
class Image;

struct IndirectCommand
{
    VkDrawIndexedIndirectCommand VulkanCommand;
    RenderHandle<RenderObject> RenderObject;
};

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

enum class PrimitiveKind
{
    Triangle, Point
};

enum class QueueKind {Graphics, Presentation, Compute};

enum class AlphaBlending {None, Over};

struct BufferCopyInfo
{
    u64 SizeBytes;
    u64 SourceOffset;
    u64 DestinationOffset;
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

// todo: not the best name
struct RenderingDetails
{
    std::vector<ImageFormat> ColorFormats;
    // todo: make it an std::optional?
    ImageFormat DepthFormat;
};

struct PipelineSpecializationInfo
{
    struct ShaderSpecialization
    {
        VkSpecializationMapEntry SpecializationEntry;
        VkShaderStageFlags ShaderStages;
    };
    std::vector<ShaderSpecialization> ShaderSpecializations;
    std::vector<u8> Buffer;
};