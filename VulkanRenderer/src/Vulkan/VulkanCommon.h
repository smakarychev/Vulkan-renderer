#pragma once

#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "Core/core.h"

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class RenderCommand;

class Buffer;
class Image;

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

enum class PrimitiveKind
{
    Triangle, Point
};

enum class ShaderKind
{
    Vertex, Pixel, Compute
};

struct ShaderModuleData
{
    VkShaderModule Module;
    ShaderKind Kind;
};

enum class QueueKind {Graphics, Presentation, Compute};

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


CREATE_ENUM_FLAGS_OPERATORS(BufferKind::Flags)

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

struct ImageData
{
    VkImage Image;
    VkImageView View;
    u32 Width;
    u32 Height;
    u32 MipMapCount{1};
};

struct ImageSubresource
{
    VkImageSubresourceRange Subresource;
};

struct ImageTransitionInfo
{
    VkImageMemoryBarrier MemoryBarrier;
    VkPipelineStageFlags SourceStage;
    VkPipelineStageFlags DestinationStage;
};

struct ImageBlitInfo
{
    const Image* SourceImage;
    const Image* DestinationImage;
    VkImageBlit* ImageBlit;
    VkFilter Filter;
};

struct BindlessDescriptorsState
{
    u32 TextureIndex{0};
};

struct PipelineBufferBarrierInfo
{
    VkPipelineStageFlags PipelineSourceMask;
    VkPipelineStageFlags PipelineDestinationMask;
    VkDependencyFlags DependencyFlags{0};
    const QueueInfo* Queue;
    const Buffer* Buffer;
    VkAccessFlags BufferSourceMask;
    VkAccessFlags BufferDestinationMask;
};

struct PipelineImageBarrierInfo
{
    VkPipelineStageFlags PipelineSourceMask;
    VkPipelineStageFlags PipelineDestinationMask;
    VkDependencyFlags DependencyFlags{0};
    const Image* Image;
    VkAccessFlags ImageSourceMask;
    VkAccessFlags ImageDestinationMask;
    VkImageLayout ImageSourceLayout;
    VkImageLayout ImageDestinationLayout;
    VkImageAspectFlags ImageAspect; 
};

using PipelineTextureBarrierInfo = PipelineImageBarrierInfo;