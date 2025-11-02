#include "rendererpch.h"

#include "Device.h"

#include "core.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <tracy/TracyVulkan.hpp>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

#include "AssetManager.h"
#include "FrameContext.h"
#include "TextureAsset.h"
#include "Rendering/Buffer/Buffer.h"
#include "Core/ProfilerContext.h"
#include "Rendering/FormatTraits.h"
#include "Utils/ContainterUtils.h"

#include "Imgui/ImguiUI.h"
#include "Rendering/DeletionQueue.h"
#include "Rendering/Commands/RenderCommands.h"
#include "Rendering/Image/ImageUtility.h"
#include "utils/CoreUtils.h"

namespace
{
    static_assert(ImageSubresourceDescription::ALL_MIPMAPS == VK_REMAINING_MIP_LEVELS,
        "Incorrect value for `ALL_MIPMAPS`");
    static_assert(ImageSubresourceDescription::ALL_LAYERS == VK_REMAINING_ARRAY_LAYERS,
        "Incorrect value for `ALL_LAYERS`");
    static_assert(SamplerCreateInfo::LOD_MAX == VK_LOD_CLAMP_NONE, "Incorrect value for `LOD_MAX`");
    
    constexpr VkFormat vulkanFormatFromFormat(Format format)
    {
        switch (format)
        {
        case Format::Undefined:            return VK_FORMAT_UNDEFINED;
        case Format::R8_UNORM:             return VK_FORMAT_R8_UNORM; 
        case Format::R8_SNORM:             return VK_FORMAT_R8_SNORM;
        case Format::R8_UINT:              return VK_FORMAT_R8_UINT;
        case Format::R8_SINT:              return VK_FORMAT_R8_SINT;
        case Format::R8_SRGB:              return VK_FORMAT_R8_SRGB;
        case Format::RG8_UNORM:            return VK_FORMAT_R8G8_UNORM;
        case Format::RG8_SNORM:            return VK_FORMAT_R8G8_SNORM;
        case Format::RG8_UINT:             return VK_FORMAT_R8G8_UINT;
        case Format::RG8_SINT:             return VK_FORMAT_R8G8_SINT;
        case Format::RG8_SRGB:             return VK_FORMAT_R8G8_SRGB;
        case Format::RGBA8_UNORM:          return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SNORM:          return VK_FORMAT_R8G8B8A8_SNORM;
        case Format::RGBA8_UINT:           return VK_FORMAT_R8G8B8A8_UINT;
        case Format::RGBA8_SINT:           return VK_FORMAT_R8G8B8A8_SINT;
        case Format::RGBA8_SRGB:           return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::R16_UNORM:            return VK_FORMAT_R16_UNORM;
        case Format::R16_SNORM:            return VK_FORMAT_R16_SNORM;
        case Format::R16_UINT:             return VK_FORMAT_R16_UINT;
        case Format::R16_SINT:             return VK_FORMAT_R16_SINT;
        case Format::R16_FLOAT:            return VK_FORMAT_R16_SFLOAT;
        case Format::RG16_UNORM:           return VK_FORMAT_R16G16_UNORM;
        case Format::RG16_SNORM:           return VK_FORMAT_R16G16_SNORM;
        case Format::RG16_UINT:            return VK_FORMAT_R16G16_UINT;
        case Format::RG16_SINT:            return VK_FORMAT_R16G16_SINT;
        case Format::RG16_FLOAT:           return VK_FORMAT_R16G16_SFLOAT;
        case Format::RGBA16_UNORM:         return VK_FORMAT_R16G16B16A16_UNORM;
        case Format::RGBA16_SNORM:         return VK_FORMAT_R16G16B16A16_SNORM;
        case Format::RGBA16_UINT:          return VK_FORMAT_R16G16B16A16_UINT;
        case Format::RGBA16_SINT:          return VK_FORMAT_R16G16B16A16_SINT;
        case Format::RGBA16_FLOAT:         return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32_UINT:             return VK_FORMAT_R32_UINT;
        case Format::R32_SINT:             return VK_FORMAT_R32_SINT;
        case Format::R32_FLOAT:            return VK_FORMAT_R32_SFLOAT;
        case Format::RG32_UINT:            return VK_FORMAT_R32G32_UINT;
        case Format::RG32_SINT:            return VK_FORMAT_R32G32_SINT;
        case Format::RG32_FLOAT:           return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGB32_UINT:           return VK_FORMAT_R32G32B32_UINT; 
        case Format::RGB32_SINT:           return VK_FORMAT_R32G32B32_SINT;
        case Format::RGB32_FLOAT:          return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::RGBA32_UINT:          return VK_FORMAT_R32G32B32A32_UINT;
        case Format::RGBA32_SINT:          return VK_FORMAT_R32G32B32A32_SINT;
        case Format::RGBA32_FLOAT:         return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::RGB10A2:              return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        case Format::R11G11B10:            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case Format::D32_FLOAT:            return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT:    return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_FLOAT_S8_UINT:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
            
        case Format::MaxVal:
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
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
        case ImageLayout::Undefined:                return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General:                  return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::Attachment:               return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        case ImageLayout::Readonly:                 return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        case ImageLayout::ColorAttachment:          return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::Present:                  return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case ImageLayout::DepthStencilAttachment:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadonly:     return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::DepthAttachment:          return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthReadonly:            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        case ImageLayout::Source:                   return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::Destination:              return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

        static const std::vector<std::pair<ImageUsage, VkImageUsageFlags>> MAPPINGS {
            {ImageUsage::Sampled,       VK_IMAGE_USAGE_SAMPLED_BIT},
            {ImageUsage::Color,         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
            {ImageUsage::Depth,         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {ImageUsage::Stencil,       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {ImageUsage::Storage,       VK_IMAGE_USAGE_STORAGE_BIT},
            {ImageUsage::Source,        VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
            {ImageUsage::Destination,   VK_IMAGE_USAGE_TRANSFER_DST_BIT}
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
        case ImageKind::Cubemap:
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
        if (kind == ImageKind::Cubemap)
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
        case ImageKind::Cubemap:
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
            return  VK_SAMPLER_ADDRESS_MODE_REPEAT;
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
        case CommandBufferKind::Primary:    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case CommandBufferKind::Secondary:  return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
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
        static const std::vector<std::pair<PipelineStage, VkPipelineStageFlags2>> MAPPINGS {
            {PipelineStage::Top,                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT},
            {PipelineStage::Indirect,               VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT},
            {PipelineStage::VertexInput,            VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT},
            {PipelineStage::IndexInput,             VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT},
            {PipelineStage::AttributeInput,         VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT},
            {PipelineStage::VertexShader,           VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT},
            {PipelineStage::HullShader,             VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT},
            {PipelineStage::DomainShader,           VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT},
            {PipelineStage::GeometryShader,         VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT},
            {PipelineStage::PixelShader,            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
            {PipelineStage::DepthEarly,             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
            {PipelineStage::DepthLate,              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT},
            {PipelineStage::ColorOutput,            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
            {PipelineStage::ComputeShader,          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {PipelineStage::Copy,                   VK_PIPELINE_STAGE_2_COPY_BIT},
            {PipelineStage::Blit,                   VK_PIPELINE_STAGE_2_BLIT_BIT},
            {PipelineStage::Resolve,                VK_PIPELINE_STAGE_2_RESOLVE_BIT},
            {PipelineStage::Clear,                  VK_PIPELINE_STAGE_2_CLEAR_BIT},
            {PipelineStage::AllTransfer,            VK_PIPELINE_STAGE_2_TRANSFER_BIT},
            {PipelineStage::AllGraphics,            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
            {PipelineStage::AllPreRasterization,    VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT},
            {PipelineStage::AllCommands,            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
            {PipelineStage::Bottom,                 VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT},
            {PipelineStage::Host,                   VK_PIPELINE_STAGE_2_HOST_BIT},
            {PipelineStage::TransformFeedback,      VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT},
            {PipelineStage::ConditionalRendering,   VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT},
        };
        
        VkPipelineStageFlags2 flags = 0;
        for (auto&& [ps, vulkanPs] : MAPPINGS)
            if (enumHasAny(stage, ps))
                flags |= vulkanPs;

        return flags;
    }

    constexpr VkAccessFlagBits2 vulkanAccessFlagsFromPipelineAccess(PipelineAccess access)
    {
        static const std::vector<std::pair<PipelineAccess, VkAccessFlagBits2>> MAPPINGS {
            {PipelineAccess::ReadIndirect,                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
            {PipelineAccess::ReadIndex,                     VK_ACCESS_2_INDEX_READ_BIT},
            {PipelineAccess::ReadAttribute,                 VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT},
            {PipelineAccess::ReadUniform,                   VK_ACCESS_2_UNIFORM_READ_BIT},
            {PipelineAccess::ReadInputAttachment,           VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadColorAttachment,           VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadDepthStencilAttachment,    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadTransfer,                  VK_ACCESS_2_TRANSFER_READ_BIT},
            {PipelineAccess::ReadHost,                      VK_ACCESS_2_HOST_READ_BIT},
            {PipelineAccess::ReadSampled,                   VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
            {PipelineAccess::ReadStorage,                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT},
            {PipelineAccess::ReadShader,                    VK_ACCESS_2_SHADER_READ_BIT},
            {PipelineAccess::ReadAll,                       VK_ACCESS_2_MEMORY_READ_BIT},
            {PipelineAccess::WriteColorAttachment,          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
            {PipelineAccess::WriteDepthStencilAttachment,   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
            {PipelineAccess::WriteTransfer,                 VK_ACCESS_2_TRANSFER_WRITE_BIT},
            {PipelineAccess::WriteHost,                     VK_ACCESS_2_HOST_WRITE_BIT},
            {PipelineAccess::WriteShader,                   VK_ACCESS_2_SHADER_WRITE_BIT},
            {PipelineAccess::WriteAll,                      VK_ACCESS_2_MEMORY_WRITE_BIT},
            {PipelineAccess::ReadFeedbackCounter,           VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT},
            {PipelineAccess::WriteFeedbackCounter,          VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT},
            {PipelineAccess::WriteFeedback,                 VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT},
            {PipelineAccess::ReadConditional,               VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT},
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
        static const std::vector<std::pair<PipelineDependencyFlags, VkDependencyFlags>> MAPPINGS {
            {PipelineDependencyFlags::ByRegion,     VK_DEPENDENCY_BY_REGION_BIT},
            {PipelineDependencyFlags::DeviceGroup,  VK_DEPENDENCY_DEVICE_GROUP_BIT},
            {PipelineDependencyFlags::FeedbackLoop, VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT},
            {PipelineDependencyFlags::LocalView,    VK_DEPENDENCY_VIEW_LOCAL_BIT},
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
        case DescriptorType::Sampler:               return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::Image:                 return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::ImageStorage:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::TexelUniform:          return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case DescriptorType::TexelStorage:          return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case DescriptorType::UniformBuffer:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::UniformBufferDynamic:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case DescriptorType::StorageBufferDynamic:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case DescriptorType::Input:                 return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
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
        {
            LOG("Device check failed with result {}: {}", vkResultToString(res), message.data());
            LOG("{}", std::stacktrace::current());
            abort();
        }
    }
}

class DeviceInternal
{
public:
#ifdef DESCRIPTOR_BUFFER
    static u32 GetDescriptorSizeBytes(DescriptorType type);
    static void WriteDescriptor(Descriptors descriptors, DescriptorSlotInfo slotInfo, u32 index,
        VkDescriptorGetInfoEXT& descriptorGetInfo);
#else
    static u32 GetFreePoolIndexFromAllocator(DescriptorArenaAllocator allocator, DescriptorPoolFlags poolFlags);
#endif
    static VmaAllocator& Allocator();
    static Buffer AllocateBuffer(const BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
      VmaAllocationCreateFlags allocationFlags);
    static VkImageView CreateVulkanImageView(const ImageSubresource& image, VkFormat format);

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        Span<const Semaphore> semaphores, Span<const PipelineStage> waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        Span<const TimelineSemaphore> semaphores,
        Span<const u64> waitValues, Span<const PipelineStage> waitStages);
    static void BindDescriptors(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);
    static VkCommandBuffer GetProfilerCommandBuffer(ProfilerContext* context);
};

class DeviceResources
{
    FRIEND_INTERNAL
    friend class DeviceInternal;

    template <typename T>
    using ResourceContainerType = DeviceSparseSet<T>;
private:
    template <typename ResourceList, typename Resource>
    constexpr auto AddToResourceList(ResourceList& list, Resource&& value);
    template <typename Resource>
    constexpr auto AddResource(Resource&& resource);
    template <typename Type>
    constexpr void RemoveResource(ResourceHandleType<Type> handle);
    template <typename Type>
    constexpr const auto& operator[](const Type& type) const;
    template <typename Type>
    constexpr auto& operator[](const Type& type);

    void MapCmdToPool(CommandBuffer cmd, CommandPool pool);
    void DestroyCmdsOfPool(CommandPool pool);
private:
    struct SwapchainResource
    {
        using ObjectType = SwapchainTag;
        VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
        VkFormat ColorFormat{};
        SwapchainDescription Description{};
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
    
    u64 m_AllocatedCount{0};
    u64 m_DeallocatedCount{0};
    
    ResourceContainerType<SwapchainResource> m_Swapchains;
    ResourceContainerType<BufferResource> m_Buffers;
    ResourceContainerType<BufferArenaResource> m_BufferArenas;
    ResourceContainerType<ImageResource> m_Images;
    ResourceContainerType<SamplerResource> m_Samplers;
    ResourceContainerType<CommandPoolResource> m_CommandPools;
    ResourceContainerType<CommandBufferResource> m_CommandBuffers;
    ResourceContainerType<DescriptorsLayoutResource> m_DescriptorLayouts;
    ResourceContainerType<DescriptorsResource> m_Descriptors;
    ResourceContainerType<DescriptorArenaAllocatorResource> m_DescriptorArenaAllocators;
    ResourceContainerType<PipelineLayoutResource> m_PipelineLayouts;
    ResourceContainerType<PipelineResource> m_Pipelines;
    ResourceContainerType<ShaderModuleResource> m_ShaderModules;
    ResourceContainerType<RenderingAttachmentResource> m_RenderingAttachments;
    ResourceContainerType<RenderingInfoResource> m_RenderingInfos;
    ResourceContainerType<FenceResource> m_Fences;
    ResourceContainerType<SemaphoreResource> m_Semaphores;
    ResourceContainerType<TimelineSemaphoreResource> m_TimelineSemaphores;
    ResourceContainerType<DependencyInfoResource> m_DependencyInfos;
    ResourceContainerType<SplitBarrierResource> m_SplitBarriers;

    std::vector<std::vector<u32>> m_CommandPoolToBuffersMap;
};

template <typename ResourceList, typename Resource>
constexpr auto DeviceResources::AddToResourceList(ResourceList& list, Resource&& value)
{
    static_assert(std::is_same_v<std::decay_t<Resource>, typename ResourceList::ValueType>);
    return list.Add(std::forward<typename ResourceList::ValueType>(value));
}

template <typename Resource>
constexpr auto DeviceResources::AddResource(Resource&& resource)
{
    m_AllocatedCount++;

    using Decayed = std::decay_t<Resource>;
    
    if constexpr(std::is_same_v<Decayed, SwapchainResource>)
        return AddToResourceList(m_Swapchains, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, BufferResource>)
        return AddToResourceList(m_Buffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, BufferArenaResource>)
        return AddToResourceList(m_BufferArenas, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, ImageResource>)
        return AddToResourceList(m_Images, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SamplerResource>)
        return AddToResourceList(m_Samplers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, CommandPoolResource>)
        return AddToResourceList(m_CommandPools, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, CommandBufferResource>)
        return AddToResourceList(m_CommandBuffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayoutResource>)
        return AddToResourceList(m_DescriptorLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorsResource>)
        return AddToResourceList(m_Descriptors, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocatorResource>)
        return AddToResourceList(m_DescriptorArenaAllocators, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, PipelineLayoutResource>)
        return AddToResourceList(m_PipelineLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, PipelineResource>)
        return AddToResourceList(m_Pipelines, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, ShaderModuleResource>)
        return AddToResourceList(m_ShaderModules, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, RenderingAttachmentResource>)
        return AddToResourceList(m_RenderingAttachments, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, RenderingInfoResource>)
        return AddToResourceList(m_RenderingInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, FenceResource>)
        return AddToResourceList(m_Fences, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SemaphoreResource>)
        return AddToResourceList(m_Semaphores, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphoreResource>)
        return AddToResourceList(m_TimelineSemaphores, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DependencyInfoResource>)
        return AddToResourceList(m_DependencyInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SplitBarrierResource>)
        return AddToResourceList(m_SplitBarriers, std::forward<Resource>(resource));
    else 
        static_assert(!sizeof(Resource), "No match for resource");
    std::unreachable();
}

template <typename Type>
constexpr void DeviceResources::RemoveResource(ResourceHandleType<Type> handle)
{
    m_DeallocatedCount++;

    using Decayed = std::decay_t<Type>;

    if constexpr(std::is_same_v<Decayed, SwapchainTag>)
        m_Swapchains.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, BufferTag>)
        m_Buffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, BufferArenaTag>)
        m_BufferArenas.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, ImageTag>)
        m_Images.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SamplerTag>)
        m_Samplers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandPoolTag>)
        m_CommandPools.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandBufferTag>)
        m_CommandBuffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayoutTag>)
        m_DescriptorLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorsTag>)
        m_Descriptors.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocatorTag>)
        m_DescriptorArenaAllocators.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, PipelineLayoutTag>)
        m_PipelineLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, PipelineTag>)
        m_Pipelines.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, ShaderModuleTag>)
        m_ShaderModules.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingAttachmentTag>)
        m_RenderingAttachments.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingInfoTag>)
        m_RenderingInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, FenceTag>)
        m_Fences.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SemaphoreTag>)
        m_Semaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphoreTag>)
        m_TimelineSemaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DependencyInfoTag>)
        m_DependencyInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SplitBarrierTag>)
        m_SplitBarriers.Remove(handle);
    else 
        static_assert(!sizeof(Type), "No match for type");
}

template <typename Type>
constexpr const auto& DeviceResources::operator[](const Type& type) const
{
    return const_cast<DeviceResources&>(*this)[type];
}

template <typename Type>
constexpr auto& DeviceResources::operator[](const Type& type)
{
    using Decayed = std::decay_t<Type>;
    
    if constexpr(std::is_same_v<Decayed, Swapchain>)
        return m_Swapchains[type];
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        return m_Buffers[type];
    else if constexpr(std::is_same_v<Decayed, BufferArena>)
        return m_BufferArenas[type];
    else if constexpr(std::is_same_v<Decayed, Image>)
        return m_Images[type];
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        return m_Samplers[type];
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        return m_CommandPools[type];
    else if constexpr(std::is_same_v<Decayed, CommandBuffer>)
        return m_CommandBuffers[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        return m_DescriptorLayouts[type];
    else if constexpr(std::is_same_v<Decayed, Descriptors>)
        return m_Descriptors[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocator>)
        return m_DescriptorArenaAllocators[type];
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        return m_PipelineLayouts[type];
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        return m_Pipelines[type];
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        return m_ShaderModules[type];
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        return m_RenderingAttachments[type];
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        return m_RenderingInfos[type];
    else if constexpr(std::is_same_v<Decayed, Fence>)
        return m_Fences[type];
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        return m_Semaphores[type];
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        return m_TimelineSemaphores[type];
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        return m_DependencyInfos[type];
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        return m_SplitBarriers[type];
    else 
        static_assert(!sizeof(Type), "No match for type");
    std::unreachable();
}

DeviceCreateInfo DeviceCreateInfo::Default(GLFWwindow* window, bool asyncCompute)
{
    DeviceCreateInfo createInfo = {};
    createInfo.AppName = "Vulkan-app";
    createInfo.ApiVersion = VK_API_VERSION_1_3;
    u32 instanceExtensionsCount = 0;
    
    const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionsCount);
    createInfo.InstanceExtensions = std::vector(instanceExtensions + 0, instanceExtensions + instanceExtensionsCount);
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
    for (auto index : m_CommandPoolToBuffersMap[pool.m_Id])
        m_CommandBuffers.Remove(index);
    m_DeallocatedCount += (u32)m_CommandPoolToBuffersMap[pool.m_Id].size(); 
    m_CommandPoolToBuffersMap[pool.m_Id].clear();
}

struct QueueInfo
{
    /* technically any family index is possible;
     * practically GPUs have only a few*/
    static constexpr u32 UNSET_FAMILY = std::numeric_limits<u32>::max();
    VkQueue Queue{VK_NULL_HANDLE};
    u32 Family{UNSET_FAMILY};
};

struct Device::State
{
    struct DeviceQueues
    {
        bool IsComplete() const
        {
            return
                Graphics.Family != QueueInfo::UNSET_FAMILY &&
                (Presentation.Family != QueueInfo::UNSET_FAMILY || s_State.Surface == VK_NULL_HANDLE) && 
                Compute.Family != QueueInfo::UNSET_FAMILY;
        }
        std::vector<u32> AsFamilySet() const
        {
            std::vector<u32> familySet{Graphics.Family};
            if (Presentation.Family != Graphics.Family)
                familySet.push_back(Presentation.Family);
            if (Compute.Family != Graphics.Family && Compute.Family != Presentation.Family)
                familySet.push_back(Compute.Family);

            return familySet;
        }
        QueueInfo GetQueueByKind(QueueKind queueKind) const
        {
            switch (queueKind)
            {
            case QueueKind::Graphics:       return Graphics;
            case QueueKind::Presentation:   return Presentation;
            case QueueKind::Compute:        return Compute;
            default:
                ASSERT(false, "Unrecognized queue kind")
                break;
            }
            std::unreachable();
        }
        u32 GetFamilyByKind(QueueKind queueKind) const
        {
            return GetQueueByKind(queueKind).Family;
        }
    public:
        QueueInfo Graphics;
        QueueInfo Presentation;
        QueueInfo Compute;
    };

    ImmediateSubmitContext GetSubmitContext();
    
    VkDevice Device{VK_NULL_HANDLE};
    DeviceResources Resources;
    VmaAllocator Allocator;
    DeviceQueues Queues;
    ::DeletionQueue DeletionQueue;
    ::DeletionQueue DummyDeletionQueue;

    ::DeletionQueue* FrameDeletionQueue{nullptr};
    
    GLFWwindow* Window{nullptr};

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

ImmediateSubmitContext Device::State::GetSubmitContext()
{
    if (!s_State.SubmitContexts.empty())
    {
        ImmediateSubmitContext ctx = SubmitContexts.back();
        SubmitContexts.pop_back();

        return ctx;
    }
    
    ImmediateSubmitContext ctx = {}; 
    ctx.CommandPool = CreateCommandPool({.QueueKind = QueueKind::Graphics});
    ctx.CommandBuffer = CreateCommandBuffer({
        .Pool = ctx.CommandPool,
        .Kind = CommandBufferKind::Primary});
    ctx.CommandList.SetCommandBuffer(ctx.CommandBuffer);
    ctx.Fence = CreateFence({});
    ctx.QueueKind = QueueKind::Graphics;

    return ctx;
}

Device::State Device::s_State = State{};

void Device::BeginFrame(FrameContext& ctx)
{
    s_State.FrameDeletionQueue = &ctx.DeletionQueue;
}

Swapchain Device::CreateSwapchain(SwapchainCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    if (s_State.Surface == VK_NULL_HANDLE)
        return {};

    static std::vector<VkSurfaceFormatKHR> desiredFormats = {{{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    static std::vector<VkPresentModeKHR> desiredPresentModes = {{
        //VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR}};
    
    SurfaceDetails surfaceDetails = getSurfaceDetails(s_State.GPU, s_State.Surface);
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

    auto chooseExtent = [](GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities)
    {
        VkExtent2D extent = capabilities.currentExtent;

        if (extent.width != std::numeric_limits<u32>::max())
            return extent;

        // indication that extent might not be same as window size
        i32 windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    
        extent.width = std::clamp(
            windowWidth, (i32)capabilities.minImageExtent.width, (i32)capabilities.maxImageExtent.width);
        extent.height = std::clamp(
            windowHeight, (i32)capabilities.minImageExtent.height, (i32)capabilities.maxImageExtent.height);

        return extent;
    };
    
    u32 imageCount = chooseImageCount(capabilities);
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = s_State.Surface;
    swapchainCreateInfo.imageColorSpace = colorFormat.colorSpace;
    swapchainCreateInfo.imageFormat = colorFormat.format;
    VkExtent2D extent = chooseExtent(s_State.Window, capabilities);
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.presentMode = presentMode;

    if (s_State.Queues.Graphics.Family == s_State.Queues.Presentation.Family)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        std::vector<u32> queueFamilies = s_State.Queues.AsFamilySet();
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = (u32)queueFamilies.size();
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();
    }
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    DeviceResources::SwapchainResource swapchainResource = {};
    swapchainResource.ColorFormat = colorFormat.format;
    deviceCheck(vkCreateSwapchainKHR(s_State.Device, &swapchainCreateInfo, nullptr, &swapchainResource.Swapchain),
        "Failed to create swapchain");

    const glm::uvec2 swapchainResolution = glm::uvec2{extent.width, extent.height};
    const glm::uvec2 drawResolution = createInfo.DrawResolution.x != 0 ?
            createInfo.DrawResolution : swapchainResolution;
    
    swapchainResource.Description = {
        .SwapchainResolution = swapchainResolution,
        .DrawResolution = drawResolution,
        .DrawFormat = createInfo.DrawFormat,
        .DepthFormat = createInfo.DepthStencilFormat,
        .DrawImage = CreateImage({
            .Description = ImageDescription{
                .Width = drawResolution.x,
                .Height = drawResolution.y,
                .Format = createInfo.DrawFormat,
                .Usage = ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color}},
            DummyDeletionQueue()),
        .DepthImage = CreateImage({
            .Description = ImageDescription{
                .Width = drawResolution.x,
                .Height = drawResolution.y,
                .Format = createInfo.DepthStencilFormat,
                .Usage = ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled}},
            DummyDeletionQueue())};
    swapchainResource.Description.Sync.assign_range(createInfo.FrameSyncs);
    if (swapchainResource.Description.Sync.empty())
    {
        swapchainResource.Description.Sync.reserve(BUFFERED_FRAMES);
        for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        {
            Fence renderFence = CreateFence({
                .IsSignaled = true});
            Semaphore renderSemaphore = CreateSemaphore();
            Semaphore presentSemaphore = CreateSemaphore();

            swapchainResource.Description.Sync.push_back({
                .RenderFence = renderFence,
                .RenderSemaphore = renderSemaphore,
                .PresentSemaphore = presentSemaphore});
        }
    }
    ASSERT(swapchainResource.Description.Sync.size() == BUFFERED_FRAMES,
        "Frame synchronization structures for swapchain have to be provided for every frame-in-flight ({})",
        BUFFERED_FRAMES)

    Swapchain swapchain = Resources().AddResource(swapchainResource);
    CreateSwapchainImages(swapchain);
    deletionQueue.Enqueue(swapchain);
    
    return swapchain;
}

void Device::Destroy(Swapchain swapchain)
{
    DestroySwapchainImages(swapchain);
    vkDestroySwapchainKHR(s_State.Device, Resources().m_Swapchains[swapchain.m_Id].Swapchain, nullptr);
    Resources().RemoveResource(swapchain);
}

void Device::CreateSwapchainImages(Swapchain swapchain)
{
    DeviceResources::SwapchainResource& swapchainResource = Resources()[swapchain];
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(s_State.Device, swapchainResource.Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(s_State.Device, swapchainResource.Swapchain, &imageCount, images.data());

    ImageDescription description = {
        .Width = swapchainResource.Description.SwapchainResolution.x,
        .Height = swapchainResource.Description.SwapchainResolution.y,
        .LayersDepth = 1,
        .Mipmaps = 1,
        .Kind = ImageKind::Image2d,
        .Usage = ImageUsage::Destination};
    std::vector<Image> colorImages(imageCount);
    
    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        DeviceResources::ImageResource imageResource = {.Image = images[i], .Description = description};
        colorImages[i] = Resources().AddResource(imageResource);
        Resources()[colorImages[i]].Views.ViewType.View = DeviceInternal::CreateVulkanImageView(
            ImageSubresource{.Image = colorImages[i], .Description = {.Mipmaps = 1, .Layers = 1}},
            swapchainResource.ColorFormat);
        Resources()[colorImages[i]].Views.ViewList = &Resources()[colorImages[i]].Views.ViewType.View;
    }

    swapchainResource.Description.ColorImages = colorImages;
}

void Device::DestroySwapchainImages(Swapchain swapchain)
{
    DeviceResources::SwapchainResource& swapchainResource = Resources()[swapchain];
    for (const auto& colorImage : swapchainResource.Description.ColorImages)
    {
        vkDestroyImageView(s_State.Device, *Resources()[colorImage].Views.ViewList, nullptr);
        Resources().RemoveResource(colorImage);
    }
    Destroy(swapchainResource.Description.DrawImage);
    Destroy(swapchainResource.Description.DepthImage);
}

u32 Device::AcquireNextImage(Swapchain swapchain, u32 frameNumber)
{
    DeviceResources::SwapchainResource& swapchainResource = Resources()[swapchain];
    const SwapchainFrameSync& frameSync = swapchainResource.Description.Sync[frameNumber];
    
    WaitForFence(frameSync.RenderFence);

    u32 imageIndex;
    VkResult res = vkAcquireNextImageKHR(s_State.Device, Resources()[swapchain].Swapchain,
        10'000'000'000, Resources()[frameSync.PresentSemaphore].Semaphore, VK_NULL_HANDLE,
        &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        return INVALID_SWAPCHAIN_IMAGE;
    
    ASSERT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image")

    ResetFence(frameSync.RenderFence);
    
    return imageIndex;
}

bool Device::Present(Swapchain swapchain, QueueKind queueKind, u32 frameNumber, u32 imageIndex)
{
    DeviceResources::SwapchainResource& swapchainResource = Resources()[swapchain];
    const SwapchainFrameSync& frameSync = swapchainResource.Description.Sync[frameNumber];
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &Resources()[swapchain].Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &Resources()[frameSync.RenderSemaphore].Semaphore;

    VkResult result = vkQueuePresentKHR(s_State.Queues.GetQueueByKind(queueKind).Queue, &presentInfo);
    
    ASSERT(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR,
        "Failed to present image")

    return result == VK_SUCCESS;
}

SwapchainDescription& Device::GetSwapchainDescription(Swapchain swapchain)
{
    return Resources()[swapchain].Description;
}

CommandBuffer Device::CreateCommandBuffer(CommandBufferCreateInfo&& createInfo)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = Resources()[createInfo.Pool].CommandPool;
    allocateInfo.level = vulkanBufferLevelFromBufferKind(createInfo.Kind);
    allocateInfo.commandBufferCount = 1;

    DeviceResources::CommandBufferResource commandBufferResource = {};
    commandBufferResource.Kind = createInfo.Kind;
    deviceCheck(vkAllocateCommandBuffers(s_State.Device, &allocateInfo, &commandBufferResource.CommandBuffer),
        "Failed to allocate command buffer");
    
    CommandBuffer cmd = Resources().AddResource(commandBufferResource);
    Resources().MapCmdToPool(cmd, createInfo.Pool);
    
    return cmd;
}

CommandPool Device::CreateCommandPool(CommandPoolCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkCommandPoolCreateFlags flags = 0;
    if (createInfo.PerBufferReset)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = flags;
    poolCreateInfo.queueFamilyIndex = s_State.Queues.GetFamilyByKind(createInfo.QueueKind);

    DeviceResources::CommandPoolResource commandPoolResource = {};
    deviceCheck(vkCreateCommandPool(s_State.Device, &poolCreateInfo, nullptr, &commandPoolResource.CommandPool),
        "Failed to create command pool");
    
    CommandPool commandPool = Resources().AddResource(commandPoolResource);
    if (commandPool.m_Id >= Resources().m_CommandPoolToBuffersMap.size())
        Resources().m_CommandPoolToBuffersMap.resize(commandPool.m_Id + 1);
    deletionQueue.Enqueue(commandPool);
    
    return commandPool;
}

void Device::Destroy(CommandPool commandPool)
{
    vkDestroyCommandPool(s_State.Device, Resources().m_CommandPools[commandPool.m_Id].CommandPool, nullptr);
    Resources().DestroyCmdsOfPool(commandPool);
    Resources().RemoveResource(commandPool);
}

void Device::ResetPool(CommandPool pool)
{
    deviceCheck(vkResetCommandPool(s_State.Device, Resources()[pool].CommandPool, 0),
        "Error while resetting command pool");
}

void Device::ResetCommandBuffer(CommandBuffer cmd)
{
    deviceCheck(vkResetCommandBuffer(Resources()[cmd].CommandBuffer, 0), "Error while resetting command buffer");
}

void Device::BeginCommandBuffer(CommandBuffer cmd)
{
    BeginCommandBuffer(cmd, CommandBufferUsage::SingleSubmit);
}

void Device::BeginCommandBuffer(CommandBuffer cmd, CommandBufferUsage usage)
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = vulkanCommandBufferFlagsFromUsage(usage);
    DeviceResources::CommandBufferResource& commandBufferResource = Resources()[cmd];
    if (commandBufferResource.Kind == CommandBufferKind::Secondary)
        beginInfo.pInheritanceInfo = &inheritanceInfo;
    
    deviceCheck(vkBeginCommandBuffer(Resources()[cmd].CommandBuffer, &beginInfo),
        "Error while beginning command buffer");
}

void Device::EndCommandBuffer(CommandBuffer cmd)
{
    deviceCheck(vkEndCommandBuffer(Resources()[cmd].CommandBuffer), "Error while ending command buffer");
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
    commandBufferSubmitInfo.commandBuffer = Resources()[cmd].CommandBuffer;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
    
    deviceCheck(vkQueueSubmit2(s_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
        fence.HasValue() ? Resources()[fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffer");
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = Resources()[cmd].CommandBuffer;
    }
        
    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = Resources()[semaphore].Semaphore;
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = DeviceInternal::CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitStages);
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(s_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
        submitSync.Fence.HasValue() ? Resources()[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

void Device::SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        Resources()[submitSync.SignalSemaphores[i]].Timeline = submitSync.SignalValues[i];

    std::vector commandBufferSubmitInfos(cmds.size(), VkCommandBufferSubmitInfo{});
    for (auto&& [i, cmd] : std::views::enumerate(cmds))
    {
        commandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfos[i].commandBuffer = Resources()[cmd].CommandBuffer;
    }

    std::vector signalSemaphoreSubmitInfos(submitSync.SignalSemaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(submitSync.SignalSemaphores))
    {
        signalSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreSubmitInfos[i].semaphore = Resources()[semaphore].Semaphore;
        signalSemaphoreSubmitInfos[i].value = submitSync.SignalValues[i];
    }
    
    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = DeviceInternal::CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitValues, submitSync.WaitStages);
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    deviceCheck(vkQueueSubmit2(s_State.Queues.GetQueueByKind(queueKind).Queue, 1, &submitInfo,
        submitSync.Fence.HasValue() ? Resources()[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");    
}

Buffer Device::CreateBuffer(BufferCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VmaAllocationCreateFlags flags = 0;
    if (enumHasAny(createInfo.Usage, BufferUsage::Mappable))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(createInfo.Usage, BufferUsage::MappableRandomAccess))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    if (createInfo.PersistentMapping)
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    Buffer buffer = DeviceInternal::AllocateBuffer(createInfo, vulkanBufferUsageFromUsage(createInfo.Usage), flags);    

    if (!createInfo.InitialData.empty())
    {
        if (enumHasAny(createInfo.Usage, BufferUsage::Mappable | BufferUsage::MappableRandomAccess))
        {
            SetBufferData(buffer, createInfo.InitialData, 0);
        }
        else
        {
            Buffer stagingBuffer = CreateStagingBuffer(createInfo.InitialData.size());
            SetBufferData(stagingBuffer, createInfo.InitialData, 0);
            ImmediateSubmit([&](RenderCommandList& cmdList)
            {
                cmdList.CopyBuffer({
                    .Source =  stagingBuffer,
                    .Destination = buffer,
                    .SizeBytes = createInfo.InitialData.size()});      
            });
            Destroy(stagingBuffer);
        }
    }

    deletionQueue.Enqueue(buffer);
    
    return buffer;
}

void Device::Destroy(Buffer buffer)
{
    const DeviceResources::BufferResource& resource = Resources().m_Buffers[buffer.m_Id];
    vmaDestroyBuffer(DeviceInternal::Allocator(), resource.Buffer, resource.Allocation);
    Resources().RemoveResource(buffer);
}

Buffer Device::CreateStagingBuffer(u64 sizeBytes)
{
    return CreateBuffer({
        .SizeBytes = sizeBytes,
        .Usage = BufferUsage::Staging | BufferUsage::Mappable,
        .PersistentMapping = true},
        DummyDeletionQueue());
}

void Device::ResizeBuffer(Buffer buffer, u64 newSize, RenderCommandList& cmdList, bool copyData)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    const BufferDescription& description = resource.Description;
    const u64 oldSize = description.SizeBytes;
    if (description.SizeBytes == newSize)
        return;

    const Buffer newBuffer = CreateBuffer({
        .SizeBytes = newSize,
        .Usage = description.Usage,
        .PersistentMapping = resource.HostAddress != nullptr},
        *s_State.FrameDeletionQueue);

    /* seems very questionable
     * after this line new Buffer will inherit lifetime of old buffer,
     * and old buffer will be deleted in frame queue
     */
    std::swap(Resources()[buffer], Resources()[newBuffer]);

    /* the source and destination are intentionally swapped */
    if (copyData)
        cmdList.CopyBuffer({
            .Source = newBuffer,
            .Destination = buffer,
            .SizeBytes = std::min(oldSize, newSize)});
}

void* Device::MapBuffer(Buffer buffer)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    void* mappedData;
    vmaMapMemory(DeviceInternal::Allocator(), resource.Allocation, &mappedData);
    return mappedData;
}

void Device::UnmapBuffer(Buffer buffer)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    vmaUnmapMemory(DeviceInternal::Allocator(), resource.Allocation);
}

void Device::SetBufferData(Buffer buffer, Span<const std::byte> data, u64 offsetBytes)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    vmaCopyMemoryToAllocation(DeviceInternal::Allocator(), data.data(), resource.Allocation, offsetBytes, data.size());
}

void Device::SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    mappedAddress = (void*)((u8*)mappedAddress + offsetBytes);
    std::memcpy(mappedAddress, data.data(), data.size());
}

void* Device::GetBufferMappedAddress(Buffer buffer)
{
    return Resources()[buffer].HostAddress;
}

usize Device::GetBufferSizeBytes(Buffer buffer)
{
    return Resources()[buffer].Description.SizeBytes;
}

const BufferDescription& Device::GetBufferDescription(Buffer buffer)
{
    return Resources()[buffer].Description;
}

u64 Device::GetDeviceAddress(Buffer buffer)
{
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = Resources()[buffer].Buffer;
    
    return vkGetBufferDeviceAddress(s_State.Device, &deviceAddressInfo);
}

BufferArena Device::CreateBufferArena(BufferArenaCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VmaVirtualBlockCreateInfo virtualBlockCreateInfo = {};
    virtualBlockCreateInfo.size = createInfo.VirtualSizeBytes;
    
    DeviceResources::BufferArenaResource bufferArenaResource = {};
    deviceCheck(vmaCreateVirtualBlock(&virtualBlockCreateInfo, &bufferArenaResource.VirtualBlock),
        "Failed to create buffer arena");
    bufferArenaResource.Buffer = createInfo.Buffer;
    bufferArenaResource.VirtualSizeBytes = createInfo.VirtualSizeBytes;
    
    BufferArena arena = Resources().AddResource(bufferArenaResource);
    deletionQueue.Enqueue(arena);
    
    return arena;
}

void Device::Destroy(BufferArena bufferArena)
{
    DeviceResources::BufferArenaResource& bufferArenaResource = Resources()[bufferArena];
    
    vmaClearVirtualBlock(bufferArenaResource.VirtualBlock);
    vmaDestroyVirtualBlock(bufferArenaResource.VirtualBlock);

    Resources().RemoveResource(bufferArena);
}

void Device::ResizeBufferArenaPhysical(BufferArena arena, u64 newSize, RenderCommandList& cmdList, bool copyData)
{
    const DeviceResources::BufferArenaResource& arenaResource = Resources()[arena];
    const DeviceResources::BufferResource& bufferResource = Resources()[arenaResource.Buffer];
    const u64 oldSize = bufferResource.Description.SizeBytes;
    if (oldSize == newSize)
        return;
    
    ResizeBuffer(arenaResource.Buffer, newSize, cmdList, copyData);
}

Buffer Device::GetBufferArenaUnderlyingBuffer(BufferArena bufferArena)
{
    return Resources()[bufferArena].Buffer;
}

u64 Device::GetBufferArenaSizeBytesPhysical(BufferArena arena)
{
    return GetBufferSizeBytes(GetBufferArenaUnderlyingBuffer(arena));
}

BufferSuballocationResult Device::BufferArenaSuballocate(BufferArena arena, u64 sizeBytes, u32 alignment)
{
    VmaVirtualAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.size = sizeBytes;
    allocationCreateInfo.alignment = alignment;
    // todo: is this ok flag to use?
    allocationCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    DeviceResources::BufferArenaResource& bufferArenaResource = Resources()[arena];
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
    
    return BufferSuballocation{
        .Buffer = bufferArenaResource.Buffer,
        .Description = {
            .SizeBytes = allocationInfo.size,
            .Offset = allocationInfo.offset},
        .Handle = (u64)allocation};
}

void Device::BufferArenaFree(BufferArena arena, const BufferSuballocation& suballocation)
{
    vmaVirtualFree(Resources()[arena].VirtualBlock, (VmaVirtualAllocation)suballocation.Handle);
}

namespace
{
    constexpr Format formatFromAssetFormat(assetLib::TextureFormat format)
    {
        switch (format)
        {
        case assetLib::TextureFormat::Unknown:
            return Format::Undefined;
        case assetLib::TextureFormat::SRGBA8:
            return Format::RGBA8_SRGB;
        case assetLib::TextureFormat::RGBA8:
            return Format::RGBA8_UNORM;
        case assetLib::TextureFormat::RGBA32:
            return Format::RGBA32_FLOAT;
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
    }
}

Image Device::CreateImage(ImageCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    Image image = {};

    if (std::holds_alternative<ImageAssetPath>(createInfo.DataSource))
        image = CreateImageFromAssetFile(createInfo, std::get<ImageAssetPath>(createInfo.DataSource));
    else if (std::holds_alternative<Span<const std::byte>>(createInfo.DataSource))
        image = CreateImageFromPixels(createInfo, std::get<Span<const std::byte>>(createInfo.DataSource));
    
    if (!enumHasAny(createInfo.Description.Usage, ImageUsage::NoDeallocation))
        deletionQueue.Enqueue(image);

    return image;
}

Image Device::CreateImageFromAssetFile(ImageCreateInfo& createInfo, ImageAssetPath assetPath)
{
    {
        // todo: invert this: asset manager should call the device, not the other way around
        Image image = AssetManager::GetImage(assetPath);
        if (image.HasValue())
        {
            createInfo.Description.Usage |= ImageUsage::NoDeallocation;
            return image;
        }
    }

    assetLib::File textureFile;
    assetLib::loadAssetFile(assetPath, textureFile);
    assetLib::TextureInfo textureInfo = assetLib::readTextureInfo(textureFile);

    Buffer imageBuffer = CreateBuffer({
        .SizeBytes = textureInfo.SizeBytes,
        .Usage = BufferUsage::Source | BufferUsage::StagingRandomAccess,
        .PersistentMapping = true},
        DummyDeletionQueue());
    DeviceResources::BufferResource& imageBufferResource = Resources()[imageBuffer];
    assetLib::unpackTexture(
        textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)imageBufferResource.HostAddress);
                    
    createInfo.Description.Format = formatFromAssetFormat(textureInfo.Format);
    createInfo.Description.Width = textureInfo.Dimensions.Width;
    createInfo.Description.Height = textureInfo.Dimensions.Height;
    createInfo.Description.Mipmaps = Images::mipmapCount({
        textureInfo.Dimensions.Width, textureInfo.Dimensions.Height});
    // todo: not always correct, should reflect in in asset file
    createInfo.Description.Kind = ImageKind::Image2d;
    createInfo.Description.LayersDepth = 1;

    Image image = CreateImageFromBuffer(createInfo, imageBuffer);
    AssetManager::AddImage(assetPath, image);
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
    
    Buffer imageBuffer = CreateBuffer({
        .SizeBytes = pixels.size(),
        .Usage = BufferUsage::Source | BufferUsage::Staging,
        .InitialData = pixels},
        DummyDeletionQueue());

    Image image = CreateImageFromBuffer(createInfo, imageBuffer);
    Destroy(imageBuffer);

    return image;
}

Image Device::CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer)
{
    Image image = AllocateImage(createInfo);
    CreateViews(ImageSubresource{.Image = image}, createInfo.Description.AdditionalViews);
    
    ImageSubresource imageSubresource = {.Image = image, .Description = {.Mipmaps = 1, .Layers = 1}};

    ImmediateSubmit([&](RenderCommandList& cmdList)
    {
        ::DeletionQueue deletionQueue = {};

        cmdList.WaitOnBarrier({
            .DependencyInfo = CreateDependencyInfo({
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = ImageLayout::Undefined,
                    .NewLayout = ImageLayout::Destination}},
            deletionQueue)});

        cmdList.CopyBufferToImage({
            .Buffer = buffer,
            .Image = image,
            .ImageSubresource = {
                .Mipmaps = 1,
                .Layers = createInfo.Description.GetLayers()}});

        ImageLayout currentLayout = ImageLayout::Destination;
        if (createInfo.CalculateMipmaps)
        {
            CreateMipmaps(image, cmdList, currentLayout);
            currentLayout = ImageLayout::Source;
            imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
        }

        cmdList.WaitOnBarrier({
            .DependencyInfo = CreateDependencyInfo({
                .LayoutTransitionInfo = LayoutTransitionInfo{
                    .ImageSubresource = imageSubresource,
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllTransfer,
                    .SourceAccess = PipelineAccess::None,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = currentLayout,
                    .NewLayout = ImageLayout::Readonly}},
            deletionQueue)});
    });
    
    return image;
}

void Device::CreateMipmaps(Image image, RenderCommandList& cmdList,
    ImageLayout currentLayout)
{
    DeviceResources::ImageResource& imageResource = Resources()[image];

    i32 width = (i32)imageResource.Description.Width;
    i32 height = (i32)imageResource.Description.Height;
    i32 depth = (i32)imageResource.Description.GetDepth();
    i8 layers = imageResource.Description.GetLayers();

    ::DeletionQueue deletionQueue = {};
    
    ImageSubresource imageSubresource = {
        .Image = image,
        .Description = {
            .MipmapBase = 0,
            .Mipmaps = 1,
            .LayerBase = 0,
            .Layers = layers}};

    LayoutTransitionInfo transitionInfo = {
        .ImageSubresource = imageSubresource,
        .SourceStage = PipelineStage::AllCommands,
        .DestinationStage = PipelineStage::AllTransfer,
        .SourceAccess = PipelineAccess::WriteAll,
        .DestinationAccess = PipelineAccess::ReadTransfer,
        .OldLayout = currentLayout,
        .NewLayout = ImageLayout::Source};

    cmdList.WaitOnBarrier({
        .DependencyInfo = CreateDependencyInfo({
            .LayoutTransitionInfo = transitionInfo}, deletionQueue)});
    for (i8 mip = 1; mip < imageResource.Description.Mipmaps; mip++)
    {
        ImageSubregion sourceSubregion = {
            .Mipmap = (u32)mip - 1,
            .Layers = (u32)layers,
            .Top = {width, height, depth}};

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
        depth = std::max(1, depth >> 1);

        ImageSubregion destinationSubregion = {
            .Mipmap = (u32)mip,
            .Layers = (u32)layers,
            .Top = {width, height, depth}};

        ImageSubresource mipmapSubresource = {
            .Image = image,
            .Description = {
                .MipmapBase = mip,
                .Mipmaps = 1,
                .Layers = layers}};

        transitionInfo = {
            .ImageSubresource = mipmapSubresource,
            .SourceStage = PipelineStage::AllTransfer,
            .DestinationStage = PipelineStage::AllTransfer,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteTransfer,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::Destination};
        cmdList.WaitOnBarrier({
            .DependencyInfo = CreateDependencyInfo({
                .LayoutTransitionInfo = transitionInfo}, deletionQueue)});

        cmdList.BlitImage({
            .Source = image,
            .Destination = image,
            .Filter = imageResource.Description.MipmapFilter,
            .SourceSubregion = sourceSubregion,
            .DestinationSubregion = destinationSubregion});
        transitionInfo = {
            .ImageSubresource = mipmapSubresource,
            .SourceStage = PipelineStage::AllCommands,
            .DestinationStage = PipelineStage::AllTransfer,
            .SourceAccess = PipelineAccess::WriteAll,
            .DestinationAccess = PipelineAccess::ReadTransfer,
            .OldLayout = ImageLayout::Destination,
            .NewLayout = ImageLayout::Source};
        cmdList.WaitOnBarrier({
            .DependencyInfo = CreateDependencyInfo({
                .LayoutTransitionInfo = transitionInfo}, deletionQueue)});
    }
}

Span<const ImageSubresourceDescription> Device::GetAdditionalImageViews(Image image)
{
    return Resources()[image].Description.AdditionalViews;
}

void Device::PreprocessCreateInfo(ImageCreateInfo& createInfo)
{
    if (std::holds_alternative<ImageAssetPath>(createInfo.DataSource))
        createInfo.Description.Usage |= ImageUsage::Destination;
    
    if (createInfo.Description.Mipmaps > 1)
        createInfo.Description.Usage |= ImageUsage::Destination | ImageUsage::Source;

    if (createInfo.Description.Kind == ImageKind::Cubemap)
        createInfo.Description.LayersDepth = 6;
    
    if (enumHasAny(createInfo.Description.Usage, ImageUsage::Readback))
        createInfo.Description.Usage |= ImageUsage::Source;
}

Image Device::AllocateImage(ImageCreateInfo& createInfo)
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
        .depth = depth};
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.imageType = vulkanImageTypeFromImageKind(createInfo.Description.Kind);
    imageCreateInfo.mipLevels = (u32)(u8)createInfo.Description.Mipmaps;
    imageCreateInfo.arrayLayers = layers;
    imageCreateInfo.flags = vulkanImageFlagsFromImageKind(createInfo.Description.Kind);

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = enumHasAny(createInfo.Description.Usage,
        ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil) ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

    DeviceResources::ImageResource imageResource = {};
    deviceCheck(vmaCreateImage(DeviceInternal::Allocator(), &imageCreateInfo, &allocationInfo,
        &imageResource.Image, &imageResource.Allocation, nullptr),
        "Failed to create image");
    imageResource.Description = createInfo.Description;

    return Resources().AddResource(imageResource);
}

void Device::Destroy(Image image)
{
    const DeviceResources::ImageResource& imageResource = Resources().m_Images[image.m_Id];
    if (imageResource.Views.ViewList == &imageResource.Views.ViewType.View)
    {
        vkDestroyImageView(s_State.Device, imageResource.Views.ViewType.View, nullptr);
    }
    else
    {
        for (u32 viewIndex = 0; viewIndex < imageResource.Views.ViewType.ViewCount; viewIndex++)
            vkDestroyImageView(s_State.Device, imageResource.Views.ViewList[viewIndex], nullptr);
        delete[] imageResource.Views.ViewList;
    }
    vmaDestroyImage(DeviceInternal::Allocator(), imageResource.Image, imageResource.Allocation);
    Resources().RemoveResource(image);
}

void Device::CreateViews(const ImageSubresource& image,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    DeviceResources::ImageResource& resource = Resources()[image.Image];
    VkFormat viewFormat = vulkanFormatFromFormat(resource.Description.Format);
    if (additionalViews.empty())
    {
        resource.Views.ViewType.View = DeviceInternal::CreateVulkanImageView(image, viewFormat);
        resource.Views.ViewList = &resource.Views.ViewType.View;
        return;
    }

    resource.Views.ViewType.ViewCount = 1 + (u32)resource.Description.AdditionalViews.size();
    resource.Views.ViewList = new VkImageView[resource.Views.ViewType.ViewCount];
    resource.Views.ViewList[0] = DeviceInternal::CreateVulkanImageView(image, viewFormat);
    for (u32 viewIndex = 0; viewIndex < additionalViews.size(); viewIndex++)
        resource.Views.ViewList[viewIndex + 1] = DeviceInternal::CreateVulkanImageView(
            ImageSubresource{.Image = image.Image, .Description = additionalViews[viewIndex]}, viewFormat);
}

ImageViewHandle Device::GetImageViewHandle(Image image, ImageSubresourceDescription subresourceDescription)
{
    if (subresourceDescription == ImageSubresourceDescription{})
        return 0;

    const ImageDescription& description = Resources()[image].Description;
    auto it = std::ranges::find(description.AdditionalViews, subresourceDescription);

    if (it != description.AdditionalViews.end())
        return ImageViewHandle{u32(it - description.AdditionalViews.begin()) + 1};
    
    LOG("ERROR: Image does not have such view subresource, returning default view");
    return ImageViewHandle{};   
}

const ImageDescription& Device::GetImageDescription(Image image)
{
    return Resources()[image].Description;
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
    
    DeviceResources::SamplerResource samplerResource = {};
    deviceCheck(vkCreateSampler(s_State.Device, &samplerCreateInfo, nullptr, &samplerResource.Sampler),
        "Failed to create depth pyramid sampler");

    Sampler sampler = Resources().AddResource(samplerResource);
    DeletionQueue().Enqueue(sampler);

    SamplerCache::Emplace(key, sampler);
    
    return sampler;
}

void Device::Destroy(Sampler sampler)
{
    vkDestroySampler(s_State.Device, Resources().m_Samplers[sampler.m_Id].Sampler, nullptr);
    Resources().RemoveResource(sampler);
}

RenderingAttachment Device::CreateRenderingAttachment(RenderingAttachmentCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    DeviceResources::RenderingAttachmentResource renderingAttachmentResource = {};

    renderingAttachmentResource.AttachmentInfo = {};
    renderingAttachmentResource.AttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderingAttachmentResource.AttachmentInfo.clearValue = VkClearValue{.color = {
        .float32 = {
            createInfo.Description.Clear.Color.F.r,
            createInfo.Description.Clear.Color.F.g,
            createInfo.Description.Clear.Color.F.b,
            createInfo.Description.Clear.Color.F.a}}};
    renderingAttachmentResource.AttachmentInfo.imageLayout = vulkanImageLayoutFromImageLayout(
        createInfo.Layout);
    renderingAttachmentResource.AttachmentInfo.imageView = Resources()[createInfo.Image].Views.ViewList[
        GetImageViewHandle(createInfo.Image, createInfo.Description.Subresource).m_Index];
    renderingAttachmentResource.AttachmentInfo.loadOp = vulkanAttachmentLoadFromAttachmentLoad(
        createInfo.Description.OnLoad);
    renderingAttachmentResource.AttachmentInfo.storeOp = vulkanAttachmentStoreFromAttachmentStore(
        createInfo.Description.OnStore);
    renderingAttachmentResource.AttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;

    RenderingAttachment renderingAttachment = Resources().AddResource(renderingAttachmentResource);
    deletionQueue.Enqueue(renderingAttachment);
    
    return renderingAttachment;
}

void Device::Destroy(RenderingAttachment renderingAttachment)
{
    Resources().RemoveResource(renderingAttachment);
}

RenderingInfo Device::CreateRenderingInfo(RenderingInfoCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    DeviceResources::RenderingInfoResource renderingInfoResource = {};
    renderingInfoResource.ColorAttachments.reserve(createInfo.ColorAttachments.size());
    
    for (auto& attachment : createInfo.ColorAttachments)
        renderingInfoResource.ColorAttachments.push_back(Resources()[attachment].AttachmentInfo);
    if (createInfo.DepthAttachment.has_value())
        renderingInfoResource.DepthAttachment = Resources()[*createInfo.DepthAttachment].AttachmentInfo;

    renderingInfoResource.RenderArea = createInfo.RenderArea;
    RenderingInfo renderingInfo = Resources().AddResource(renderingInfoResource);
    deletionQueue.Enqueue(renderingInfo);
    
    return renderingInfo;
}

void Device::Destroy(RenderingInfo renderingInfo)
{
    Resources().RemoveResource(renderingInfo);
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
        descriptorsLayouts.push_back(Resources()[descriptorLayout].Layout);
    
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pushConstantRangeCount = (u32)pushConstantRanges.size();
    layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
    layoutCreateInfo.setLayoutCount = (u32)descriptorsLayouts.size();
    layoutCreateInfo.pSetLayouts = descriptorsLayouts.data();

    DeviceResources::PipelineLayoutResource pipelineLayoutResource = {};
    pipelineLayoutResource.PushConstants = pushConstantRanges;
    deviceCheck(vkCreatePipelineLayout(s_State.Device, &layoutCreateInfo, nullptr, &pipelineLayoutResource.Layout),
        "Failed to create pipeline layout");

    PipelineLayout layout = Resources().AddResource(pipelineLayoutResource);
    deletionQueue.Enqueue(layout);

    return layout;
}

void Device::Destroy(PipelineLayout pipelineLayout)
{
    vkDestroyPipelineLayout(s_State.Device, Resources().m_PipelineLayouts[pipelineLayout.m_Id].Layout, nullptr);
    Resources().RemoveResource(pipelineLayout);
}

Pipeline Device::CreatePipeline(PipelineCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkPipelineLayout layout = Resources()[createInfo.PipelineLayout].Layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    shaders.reserve(createInfo.Shaders.size());
    for (auto&& [i, shader] : std::views::enumerate(createInfo.Shaders))
    {
        auto& module = Resources()[shader];

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
                    .size = specialization.SizeBytes});

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
        
        DeviceResources::PipelineResource pipelineResource = {};
        deviceCheck(vkCreateComputePipelines(s_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create compute pipeline");
        pipeline = Resources().AddResource(pipelineResource);
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
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX});

        for (auto& attribute : createInfo.VertexDescription.Attributes)
            attributes.push_back({
                .location = attribute.Index,
                .binding = attribute.BindingIndex,
                .format = vulkanFormatFromFormat(attribute.Format),
                .offset = attribute.OffsetBytes});
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
             createInfo.DepthMode == DepthMode::Read) ? VK_FALSE : VK_TRUE;
        depthStencilState.depthCompareOp = createInfo.DepthTest == DepthTest::GreaterOrEqual ?
            VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_EQUAL;
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

        DeviceResources::PipelineResource pipelineResource = {};
        deviceCheck(vkCreateGraphicsPipelines(s_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create graphics pipeline");
        pipeline = Resources().AddResource(pipelineResource);
    }
    deletionQueue.Enqueue(pipeline);
    
    return pipeline;
}

void Device::Destroy(Pipeline pipeline)
{
    vkDestroyPipeline(s_State.Device, Resources().m_Pipelines[pipeline.m_Id].Pipeline, nullptr);
    Resources().RemoveResource(pipeline);
}

ShaderModule Device::CreateShaderModule(ShaderModuleCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = createInfo.Source.size();
    moduleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.Source.data());

    DeviceResources::ShaderModuleResource shaderModuleResource = {};
    deviceCheck(vkCreateShaderModule(s_State.Device, &moduleCreateInfo, nullptr, &shaderModuleResource.Module),
         "Failed to create shader module");
    
    ShaderModule module = Resources().AddResource(shaderModuleResource);
    deletionQueue.Enqueue(module);
    
    return module;
}

void Device::Destroy(ShaderModule shaderModule)
{
    vkDestroyShaderModule(s_State.Device, Resources().m_ShaderModules[shaderModule.m_Id].Module, nullptr);
    Resources().RemoveResource(shaderModule);
}

DescriptorsLayout Device::CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo)
{
    ASSERT(createInfo.BindingFlags.size() == createInfo.Bindings.size(),
        "If any element of binding flags is set, every element has to be set")

    const DescriptorLayoutCache::CacheKey key = DescriptorLayoutCache::CreateCacheKey(createInfo);
    DescriptorsLayout cached = DescriptorLayoutCache::Find(key);
    if (cached.HasValue())
        return cached;
    
    std::vector<VkDescriptorBindingFlags> bindingFlags;
    bindingFlags.reserve(createInfo.BindingFlags.size());
    
#ifdef DESCRIPTOR_BUFFER
    for (auto flag : createInfo.BindingFlags)
        bindingFlags.push_back(vulkanDescriptorBindingFlagsFromDescriptorFlags(flag));
#else
    for (auto flag : createInfo.BindingFlags)
    {
        if (enumHasAny(flag, DescriptorFlags::VariableCount))
            flag |= DescriptorFlags::UpdateAfterBind |
                    DescriptorFlags::UpdateUnusedPending;
        bindingFlags.push_back(vulkanDescriptorBindingFlagsFromDescriptorFlags(flag | DescriptorFlags::PartiallyBound));
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
            .pImmutableSamplers = nullptr});

        layoutHasImmutableSamplers |= binding.ImmutableSampler.HasValue();
        if (binding.ImmutableSampler.HasValue())
            bindings.back().pImmutableSamplers = &Resources()[binding.ImmutableSampler].Sampler;
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

    DeviceResources::DescriptorsLayoutResource descriptorSetLayoutResource = {};
    deviceCheck(vkCreateDescriptorSetLayout(s_State.Device, &layoutCreateInfo, nullptr,
        &descriptorSetLayoutResource.Layout), "Failed to create descriptor set layout");
    
    DescriptorsLayout layout = Resources().AddResource(descriptorSetLayoutResource);
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
    static const DescriptorsLayout EMPTY_LAYOUT =
        CreateDescriptorsLayout({});
#endif

    return EMPTY_LAYOUT;
}

void Device::Destroy(DescriptorsLayout layout)
{
    vkDestroyDescriptorSetLayout(s_State.Device, Resources().m_DescriptorLayouts[layout.m_Id].Layout, nullptr);
    Resources().RemoveResource(layout);
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

    DeviceResources::DescriptorArenaAllocatorResource allocatorResource = {};
    allocatorResource.DescriptorSet = createInfo.DescriptorSet;
    allocatorResource.DescriptorBufferUsage = descriptorBufferUsage;
    allocatorResource.Residence = createInfo.Residence;
    allocatorResource.SizeBytes = arenaSizeBytes;
    allocatorResource.Descriptors.reserve(createInfo.DescriptorCount);
    const BufferCreateInfo arenaCreateInfo = {.SizeBytes = arenaSizeBytes, .PersistentMapping = true};
    allocatorResource.Arena = DeviceInternal::AllocateBuffer(arenaCreateInfo, usageFlags, allocationFlags);
    allocatorResource.DeviceAddress = GetDeviceAddress(allocatorResource.Arena);
    allocatorResource.MappedAddress = GetBufferMappedAddress(allocatorResource.Arena);

    DescriptorArenaAllocator allocator = Resources().AddResource(allocatorResource);
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void Device::Destroy(DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator(allocator);
    Destroy(Resources()[allocator].Arena);
    Resources().RemoveResource(allocator);
}

std::optional<Descriptors> Device::AllocateDescriptors(DescriptorArenaAllocator allocator,
    DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings)
{
    DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
    auto& descriptorBufferProps = s_State.GPUDescriptorBufferProperties;

    /* if we have bindless binding, we have to calculate layout size as a sum of bindings sizes */
    u64 layoutSizeBytes = 0;
    if (bindings.BindlessCount == 0)
    {
        vkGetDescriptorSetLayoutSizeEXT(s_State.Device, Resources()[layout].Layout, &layoutSizeBytes);    
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
    
    layoutSizeBytes = CoreUtils::align(layoutSizeBytes, descriptorBufferProps.descriptorBufferOffsetAlignment);
    if (layoutSizeBytes + allocatorResource.CurrentOffset > allocatorResource.SizeBytes)
        return {};

    std::vector<u64> bindingOffsets(bindings.Bindings.size());
    for (u32 offsetIndex = 0; offsetIndex < bindingOffsets.size(); offsetIndex++)
    {
        auto& binding = bindings.Bindings[offsetIndex];
        vkGetDescriptorSetLayoutBindingOffsetEXT(s_State.Device, Resources()[layout].Layout, binding.Binding,
            &bindingOffsets[offsetIndex]);
        bindingOffsets[offsetIndex] += allocatorResource.CurrentOffset;
    }

    DeviceResources::DescriptorsResource descriptorsResource = {};
    descriptorsResource.Offsets = bindingOffsets;
    descriptorsResource.SizeBytes = layoutSizeBytes;
    descriptorsResource.Allocator = allocator;
    
    Descriptors descriptors = Resources().AddResource(descriptorsResource);
    allocatorResource.Descriptors.push_back(descriptors);

    allocatorResource.CurrentOffset += layoutSizeBytes;
    
    return descriptors;
}

void Device::ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator)
{
    DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
    for (Descriptors descriptors : allocatorResource.Descriptors)
        Resources().RemoveResource(descriptors);
    allocatorResource.Descriptors.clear();
    
    allocatorResource.CurrentOffset = 0;
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type != DescriptorType::TexelStorage && type != DescriptorType::TexelUniform,
        "Texel buffers require format information")
    ASSERT(type != DescriptorType::StorageBufferDynamic && type != DescriptorType::UniformBufferDynamic,
        "Dynamic buffers are not supported when using descriptor buffer")

    const DeviceResources::BufferResource& bufferResource = Resources()[buffer.Buffer];
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = bufferResource.Buffer;
    u64 deviceAddress = vkGetBufferDeviceAddress(s_State.Device, &deviceAddressInfo);

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

    DeviceInternal::WriteDescriptor(descriptors, slotInfo, index, descriptorGetInfo);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    ASSERT(type == DescriptorType::Sampler)
    
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    descriptorGetInfo.data.pSampler = &Resources()[sampler].Sampler;
    DeviceInternal::WriteDescriptor(descriptors, slotInfo, 0, descriptorGetInfo);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.imageView =
        Resources()[image.Image].Views.ViewList[GetImageViewHandle(image.Image, image.Description).m_Index];
    descriptorImageInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);
    descriptorGetInfo.data.pSampledImage = &descriptorImageInfo;

    DeviceInternal::WriteDescriptor(descriptors, slotInfo, index, descriptorGetInfo);
}
#else
DescriptorArenaAllocator Device::CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo,
    ::DeletionQueue& deletionQueue)
{
    DeviceResources::DescriptorArenaAllocatorResource descriptorAllocatorResource = {};
    descriptorAllocatorResource.MaxSetsPerPool = createInfo.DescriptorCount;
    descriptorAllocatorResource.Descriptors.reserve(createInfo.DescriptorCount);

    DescriptorArenaAllocator allocator = Resources().AddResource(descriptorAllocatorResource);
    deletionQueue.Enqueue(allocator);

    return allocator;
}

void Device::Destroy(DescriptorArenaAllocator allocator)
{
    ResetDescriptorArenaAllocator(allocator);

    const DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkDestroyDescriptorPool(s_State.Device, pool.Pool, nullptr);
    for (auto& pool : allocatorResource.UsedPools)
        vkDestroyDescriptorPool(s_State.Device, pool.Pool, nullptr);

    Resources().RemoveResource(allocator);
}

std::optional<Descriptors> Device::AllocateDescriptors(DescriptorArenaAllocator allocator, DescriptorsLayout layout,
    const DescriptorAllocatorAllocationBindings& bindings)
{
    const bool hasBindless = bindings.BindlessCount > 0;
    const DescriptorPoolFlags poolFlags = hasBindless ?
        DescriptorPoolFlags::UpdateAfterBind : DescriptorPoolFlags::None;
    
    DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
    u32 poolIndex = DeviceInternal::GetFreePoolIndexFromAllocator(allocator, poolFlags);
    VkDescriptorPool pool = allocatorResource.FreePools[poolIndex].Pool;

    VkDescriptorSetVariableDescriptorCountAllocateInfo vulkanVariableBindingCounts = {};
    vulkanVariableBindingCounts.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    vulkanVariableBindingCounts.descriptorSetCount = (u32)hasBindless;
    vulkanVariableBindingCounts.pDescriptorCounts = &bindings.BindlessCount;
    
    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &Resources()[layout].Layout;
    allocateInfo.pNext = &vulkanVariableBindingCounts;

    DeviceResources::DescriptorsResource descriptorSetResource = {};
    vkAllocateDescriptorSets(s_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet);
    descriptorSetResource.Pool = pool;

    if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
    {
        allocatorResource.UsedPools.push_back(allocatorResource.FreePools[poolIndex]);
        allocatorResource.FreePools.erase(allocatorResource.FreePools.begin() + poolIndex);

        poolIndex = DeviceInternal::GetFreePoolIndexFromAllocator(allocator, poolFlags);
        pool = allocatorResource.FreePools[poolIndex].Pool;
        allocateInfo.descriptorPool = pool;
        allocateInfo.pSetLayouts = &Resources()[descriptorSetResource.Layout].Layout;
        vkAllocateDescriptorSets(s_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet);
        if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
            return std::nullopt;
        
        descriptorSetResource.Pool = pool;
    }

    const Descriptors set = Resources().AddResource(descriptorSetResource);
    allocatorResource.Descriptors.push_back(set);
    
    return set;
}

void Device::ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator)
{
    DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkResetDescriptorPool(s_State.Device, pool.Pool, 0);
    for (auto pool : allocatorResource.UsedPools)
    {
        vkResetDescriptorPool(s_State.Device, pool.Pool, 0);
        allocatorResource.FreePools.push_back(pool);
    }
    allocatorResource.UsedPools.clear();
    for (Descriptors set : allocatorResource.Descriptors)
        Resources().RemoveResource(set);
    allocatorResource.Descriptors.clear();
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const BufferSubresource& buffer, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = Resources()[buffer.Buffer].Buffer;
    descriptorBufferInfo.offset = buffer.Description.Offset;
    descriptorBufferInfo.range = buffer.Description.SizeBytes;
    
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.dstSet = Resources()[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pBufferInfo = &descriptorBufferInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(s_State.Device, 1, &write, 0, nullptr);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = Resources()[sampler].Sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = Resources()[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;

    vkUpdateDescriptorSets(s_State.Device, 1, &write, 0, nullptr);
}

void Device::UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
    const ImageSubresource& image, ImageLayout layout, u32 index)
{
    auto&& [slot, type] = slotInfo;
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.imageView =
        Resources()[image.Image].Views.ViewList[GetImageViewHandle(image.Image, image.Description).m_Index];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = Resources()[descriptors].DescriptorSet;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = index;

    vkUpdateDescriptorSets(s_State.Device, 1, &write, 0, nullptr);
}
#endif

Fence Device::CreateFence(FenceCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (createInfo.IsSignaled)
        fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        fenceCreateInfo.flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    DeviceResources::FenceResource fenceResource = {};    
    deviceCheck(vkCreateFence(s_State.Device, &fenceCreateInfo, nullptr, &fenceResource.Fence),
        "Failed to create fence");

    Fence fence = Resources().AddResource(fenceResource);
    deletionQueue.Enqueue(fence);

    return fence;
}

void Device::Destroy(Fence fence)
{
    vkDestroyFence(s_State.Device, Resources().m_Fences[fence.m_Id].Fence, nullptr);
    Resources().RemoveResource(fence);
}

void Device::WaitForFence(Fence fence)
{
    deviceCheck(vkWaitForFences(s_State.Device, 1, &Resources()[fence].Fence, true, 10'000'000'000),
        "Error while waiting for fences");
}

bool Device::CheckFence(Fence fence)
{
    const VkResult result = vkGetFenceStatus(s_State.Device, Resources()[fence].Fence);
    return result == VK_SUCCESS;
}

void Device::ResetFence(Fence fence)
{
    deviceCheck(vkResetFences(s_State.Device, 1, &Resources()[fence].Fence), "Error while resetting fences");
}

Semaphore Device::CreateSemaphore(::DeletionQueue& deletionQueue)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    DeviceResources::SemaphoreResource semaphoreResource = {};
    deviceCheck(vkCreateSemaphore(s_State.Device, &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore),
        "Failed to create semaphore");
    
    Semaphore semaphore = Resources().AddResource(semaphoreResource);
    deletionQueue.Enqueue(semaphore);
    
    return semaphore;
}

void Device::Destroy(TimelineSemaphore semaphore)
{
    vkDestroySemaphore(s_State.Device, Resources().m_Semaphores[semaphore.m_Id].Semaphore, nullptr);
    Resources().RemoveResource(semaphore);
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

    DeviceResources::TimelineSemaphoreResource semaphoreResource = {};
    vkCreateSemaphore(s_State.Device, &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore);
    semaphoreResource.Timeline = createInfo.InitialValue;

    TimelineSemaphore semaphore = Resources().AddResource(semaphoreResource);
    deletionQueue.Enqueue(semaphore);
    
    return semaphore;
}

void Device::Destroy(Semaphore semaphore)
{
    vkDestroySemaphore(s_State.Device, Resources().m_Semaphores[semaphore.m_Id].Semaphore, nullptr);
    Resources().RemoveResource(semaphore);
}

void Device::TimelineSemaphoreWaitCPU(TimelineSemaphore semaphore, u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &Resources()[semaphore].Semaphore;
    waitInfo.pValues = &value;
    
    deviceCheck(vkWaitSemaphores(s_State.Device, &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void Device::TimelineSemaphoreSignalCPU(TimelineSemaphore semaphore, u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = Resources()[semaphore].Semaphore;
    signalInfo.value = value;

    deviceCheck(vkSignalSemaphore(s_State.Device, &signalInfo),
        "Failed to signal semaphore");

    Resources()[semaphore].Timeline = value;
}

DependencyInfo Device::CreateDependencyInfo(DependencyInfoCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    DeviceResources::DependencyInfoResource dependencyInfoResource = {};
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
        const DeviceResources::ImageResource& image =
            Resources()[createInfo.LayoutTransitionInfo->ImageSubresource.Image];
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
        imageMemoryBarrier.image = Resources()[createInfo.LayoutTransitionInfo->ImageSubresource.Image].Image;
        imageMemoryBarrier.subresourceRange = {
            .aspectMask = vulkanImageAspectFromImageUsage(image.Description.Usage),
            .baseMipLevel = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.MipmapBase,
            .levelCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Mipmaps,
            .baseArrayLayer = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.LayerBase,
            .layerCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Layers};

        dependencyInfoResource.LayoutDependency = imageMemoryBarrier;
    }

    DependencyInfo dependencyInfo = Resources().AddResource(dependencyInfoResource);
    deletionQueue.Enqueue(dependencyInfo);
    
    return dependencyInfo;
}

void Device::Destroy(DependencyInfo dependencyInfo)
{
    Resources().RemoveResource(dependencyInfo);
}

SplitBarrier Device::CreateSplitBarrier(::DeletionQueue& deletionQueue)
{
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    DeviceResources::SplitBarrierResource splitBarrierResource = {};
    deviceCheck(vkCreateEvent(s_State.Device, &eventCreateInfo, nullptr, &splitBarrierResource.Event),
        "Failed to create split barrier");

    SplitBarrier splitBarrier = Resources().AddResource(splitBarrierResource);
    deletionQueue.Enqueue(splitBarrier);
    
    return splitBarrier;
}

void Device::Destroy(SplitBarrier splitBarrier)
{
    vkDestroyEvent(s_State.Device, Resources().m_SplitBarriers[splitBarrier.m_Id].Event, nullptr);
    Resources().RemoveResource(splitBarrier);
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
            [](const char* req) { LOG("Unsupported instance extension: {}\n", req); });
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
            [](const char* req) { LOG("Unsupported validation layer: {}\n", req); });
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
    deviceCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &s_State.Instance),
        "Failed to create instance\n");

    volkLoadInstance(s_State.Instance);
}

void Device::CreateSurface(const DeviceCreateInfo& createInfo)
{
    if (createInfo.Window == nullptr)
    {
        LOG("Running Vulkan without swapchain: window pointer is unset");
        return;
    }

    s_State.Window = createInfo.Window;
    deviceCheck(glfwCreateWindowSurface(s_State.Instance, createInfo.Window, nullptr, &s_State.Surface),
        "Failed to create surface\n");
}

void Device::ChooseGPU(const DeviceCreateInfo& createInfo)
{
    auto findQueueFamilies = [](VkPhysicalDevice gpu, bool dedicatedCompute)
    {
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());
    
        State::DeviceQueues queues = {};
    
        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                queues.Graphics.Family == QueueInfo::UNSET_FAMILY)
                queues.Graphics.Family = i;

            if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && queues.Compute.Family == QueueInfo::UNSET_FAMILY)
                if (!dedicatedCompute || !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    queues.Compute.Family = i;

            if (s_State.Surface != VK_NULL_HANDLE)
            {
                VkBool32 isPresentationSupported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, s_State.Surface, &isPresentationSupported);
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
                [](const char* req) { LOG("Unsupported device extension: {}\n", req); });
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
            deviceVulkan12Features.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            deviceVulkan12Features.pNext = &deviceVulkan11Features;

            VkPhysicalDeviceVulkan13Features deviceVulkan13Features = {};
            deviceVulkan13Features.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
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
                deviceVulkan12Features.samplerFilterMinmax == VK_TRUE &&
                deviceVulkan12Features.drawIndirectCount == VK_TRUE &&
                deviceVulkan12Features.subgroupBroadcastDynamicId == VK_TRUE &&
                deviceVulkan12Features.shaderFloat16 == VK_TRUE &&
                deviceVulkan12Features.shaderInt8 == VK_TRUE &&
                deviceVulkan12Features.storageBuffer8BitAccess  == VK_TRUE &&
                deviceVulkan12Features.uniformAndStorageBuffer8BitAccess == VK_TRUE &&
                deviceVulkan12Features.shaderSubgroupExtendedTypes == VK_TRUE &&
                deviceVulkan12Features.shaderBufferInt64Atomics == VK_TRUE &&
                deviceVulkan12Features.timelineSemaphore == VK_TRUE &&
                deviceVulkan12Features.bufferDeviceAddress == VK_TRUE &&
                deviceVulkan12Features.scalarBlockLayout == VK_TRUE &&
                deviceVulkan13Features.dynamicRendering == VK_TRUE &&
                deviceVulkan13Features.synchronization2 == VK_TRUE &&
                conditionalRenderingFeaturesExt.conditionalRendering == VK_TRUE &&
                physicalDeviceIndexTypeUint8FeaturesExt.indexTypeUint8 == VK_TRUE;
#ifdef DESCRIPTOR_BUFFER
            suitable = suitable && physicalDeviceDescriptorBufferFeaturesExt.descriptorBuffer == VK_TRUE;
#endif
            
            return suitable;
        };
        
        State::DeviceQueues deviceQueues = findQueueFamilies(gpu, createInfo.AsyncCompute);
        if (!deviceQueues.IsComplete())
            return false;

        bool isEveryExtensionSupported = checkGPUExtensions(gpu, createInfo);
        if (!isEveryExtensionSupported)
            return false;

        if (s_State.Surface != VK_NULL_HANDLE)
        {
            SurfaceDetails surfaceDetails = getSurfaceDetails(gpu, s_State.Surface);
            if (!surfaceDetails.IsSufficient())
                return false;
        }

        bool isEveryFeatureSupported = checkGPUFeatures(gpu);
        if (!isEveryFeatureSupported)
            return false;
    
        return true;
    };
    
    u32 availableGPUCount = 0;
    vkEnumeratePhysicalDevices(s_State.Instance, &availableGPUCount, nullptr);
    std::vector<VkPhysicalDevice> availableGPUs(availableGPUCount);
    vkEnumeratePhysicalDevices(s_State.Instance, &availableGPUCount, availableGPUs.data());

    for (auto candidate : availableGPUs)
    {
        if (isGPUSuitable(candidate, createInfo))
        {
            s_State.GPU = candidate;
            s_State.Queues = findQueueFamilies(candidate, createInfo.AsyncCompute);
            break;
        }
    }
    
    ASSERT(s_State.GPU != VK_NULL_HANDLE, "Failed to find suitable gpu device")

    s_State.GPUDescriptorIndexingProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    s_State.GPUSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    s_State.GPUSubgroupProperties.pNext = &s_State.GPUDescriptorIndexingProperties;

    s_State.GPUDescriptorBufferProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    s_State.GPUDescriptorBufferProperties.pNext = &s_State.GPUSubgroupProperties;
    
    VkPhysicalDeviceProperties2 deviceProperties2 = {};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &s_State.GPUDescriptorBufferProperties;
    vkGetPhysicalDeviceProperties2(s_State.GPU, &deviceProperties2);
    s_State.GPUProperties = deviceProperties2.properties;
}

void Device::CreateDevice(const DeviceCreateInfo& createInfo)
{
    std::vector<u32> queueFamilies = s_State.Queues.AsFamilySet();
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
    vulkan12Features.storageBuffer8BitAccess  = VK_TRUE;
    vulkan12Features.shaderSubgroupExtendedTypes  = VK_TRUE;
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

    deviceCheck(vkCreateDevice(s_State.GPU, &deviceCreateInfo, nullptr, &s_State.Device),
        "Failed to create device\n");

    volkLoadDevice(s_State.Device);
}

void Device::RetrieveDeviceQueues()
{
    s_State.Queues.Graphics.Queue = {};
    s_State.Queues.Presentation.Queue = {};
    s_State.Queues.Compute.Queue = {};

    vkGetDeviceQueue(s_State.Device, s_State.Queues.Graphics.Family, 0, &s_State.Queues.Graphics.Queue);
    if (s_State.Surface != VK_NULL_HANDLE)
        vkGetDeviceQueue(s_State.Device, s_State.Queues.Presentation.Family, 0, &s_State.Queues.Presentation.Queue);
    vkGetDeviceQueue(s_State.Device, s_State.Queues.Compute.Family, 0, &s_State.Queues.Compute.Queue);
}

namespace
{
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        LOG("VALIDATION LAYER: {}", callbackData->pMessage);
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
        s_State.Instance, &debugUtilsMessengerCreateInfo, nullptr, &s_State.DebugUtilsMessenger);
}

void Device::DestroyDebugUtilsMessenger()
{
    vkDestroyDebugUtilsMessengerEXT(s_State.Instance, s_State.DebugUtilsMessenger, nullptr);
}

void Device::WaitIdle()
{
    vkDeviceWaitIdle(s_State.Device);
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
    vmaCreateInfo.instance = s_State.Instance;
    vmaCreateInfo.physicalDevice = s_State.GPU;
    vmaCreateInfo.device = s_State.Device;
    vmaCreateInfo.pVulkanFunctions = (const VmaVulkanFunctions*)&vulkanFunctions;
    vmaCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
    vmaCreateAllocator(&vmaCreateInfo, &s_State.Allocator);

    s_State.DummyDeletionQueue.m_IsDummy = true;

    if constexpr(std::is_same_v<DeviceFreelist<Image>, DeviceResources::ResourceContainerType<Image>>)
    {
        Resources().m_Images.SetOnResizeCallback(
            [](DeviceResources::ImageResource* oldMem, DeviceResources::ImageResource* newMem)
            {
                u32 imageCount = Resources().m_Images.Capacity();
                for (u32 imageIndex = 0; imageIndex < imageCount; imageIndex++)
                {
                    auto& resource = Resources().m_Images[imageIndex];
                    if (resource.Views.ViewList == &resource.Views.ViewType.View)
                        *(VkImageView**)((u8*)newMem + ((u8*)&resource.Views.ViewList - (u8*)oldMem)) =
                            (VkImageView*)((u8*)newMem + ((u8*)&resource.Views.ViewType.View - (u8*)oldMem));
                }
            });
    }
    if constexpr(std::is_same_v<DeviceSparseSet<Image>, DeviceResources::ResourceContainerType<Image>>)
    {
        Resources().m_Images.SetOnSwapCallback(
            [](DeviceResources::ImageResource& a, DeviceResources::ImageResource& b)
            {
                /* resource `a` will be deleted right after, so we just do not touch it*/
                if (b.Views.ViewList == &b.Views.ViewType.View)
                    b.Views.ViewList = &a.Views.ViewType.View;
            });
    }

    if (s_State.Surface != VK_NULL_HANDLE)
        InitImGuiUI();
}

void Device::Shutdown()
{
    vkDeviceWaitIdle(s_State.Device);

    ShutdownImGuiUI();
    s_State.DeletionQueue.Flush();
    ShutdownResources();

#ifdef VULKAN_VAL_LAYERS
    DestroyDebugUtilsMessenger();
#endif
    
    vkDestroyDevice(s_State.Device, nullptr);
    vkDestroySurfaceKHR(s_State.Instance, s_State.Surface, nullptr);
    vkDestroyInstance(s_State.Instance, nullptr);
}

DeletionQueue& Device::DeletionQueue()
{
    return s_State.DeletionQueue;
}

DeletionQueue& Device::DummyDeletionQueue()
{
    return s_State.DummyDeletionQueue;
}

u64 Device::GetUniformBufferAlignment()
{
    return s_State.GPUProperties.limits.minUniformBufferOffsetAlignment;
}

f32 Device::GetAnisotropyLevel()
{
    return s_State.GPUProperties.limits.maxSamplerAnisotropy;
}

u32 Device::GetMaxIndexingImages()
{
    return s_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
}

u32 Device::GetMaxIndexingUniformBuffers()
{
    return s_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
}

u32 Device::GetMaxIndexingUniformBuffersDynamic()
{
    return s_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
}

u32 Device::GetMaxIndexingStorageBuffers()
{
    return s_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
}

u32 Device::GetMaxIndexingStorageBuffersDynamic()
{
    return s_State.GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
}

u32 Device::GetSubgroupSize()
{
    return s_State.GPUSubgroupProperties.subgroupSize;
}

ImmediateSubmitContext Device::GetSubmitContext()
{
    auto ctx = s_State.GetSubmitContext();
    BeginCommandBuffer(ctx.CommandBuffer);

    return ctx;
}

void Device::FreeSubmitContext(const ImmediateSubmitContext& ctx)
{
    EndCommandBuffer(ctx.CommandBuffer);
    SubmitCommandBuffer(ctx.CommandBuffer, ctx.QueueKind, ctx.Fence);
    WaitForFence(ctx.Fence);
    ResetFence(ctx.Fence);
    ResetPool(ctx.CommandPool);
    
    s_State.SubmitContexts.push_back(ctx);
}

DeviceResources& Device::Resources()
{
    return s_State.Resources;
}

void Device::ShutdownResources()
{
    vmaDestroyAllocator(s_State.Allocator);

    ASSERT(Resources().m_AllocatedCount == Resources().m_DeallocatedCount,
        "Not all driver resources are destroyed")
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
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = (u32)poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(s_State.Device, &poolCreateInfo, nullptr, &s_State.ImGuiPool);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    
    ImGui_ImplGlfw_InitForVulkan(s_State.Window, true);

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = s_State.Instance;
    imguiInitInfo.PhysicalDevice = s_State.GPU;
    imguiInitInfo.Device = s_State.Device;
    imguiInitInfo.QueueFamily = s_State.Queues.Graphics.Family;
    imguiInitInfo.Queue = s_State.Queues.Graphics.Queue;
    imguiInitInfo.DescriptorPool = s_State.ImGuiPool;
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
    }, s_State.Instance);
    ImGui_ImplVulkan_Init(&imguiInitInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void Device::ShutdownImGuiUI()
{
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        ImGuiUI::ClearFrameResources(i);
        
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(s_State.Device, s_State.ImGuiPool, nullptr);
}

ProfilerContext::Ctx Device::CreateTracyGraphicsContext(CommandBuffer cmd)
{
    TracyVkCtx context = TracyVkContext(s_State.GPU, s_State.Device,
        s_State.Queues.Graphics.Queue, Resources()[cmd].CommandBuffer)
    return context;
}

void Device::DestroyTracyGraphicsContext(ProfilerContext::Ctx context)
{
    TracyVkDestroy((TracyVkCtx)context)
}

VkCommandBuffer DeviceInternal::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return Device::Resources()[context->m_GraphicsCommandBuffers[context->m_CurrentFrame]].CommandBuffer;
}

void Device::CreateGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu, const SourceLocationData& sourceLocationData)
{
    static_assert(sizeof(zoneGpu.Impl) >= sizeof(tracy::VkCtxScope));
    new (&zoneGpu.Impl) tracy::VkCtxScope(
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
    ImageViewHandle viewHandle = GetImageViewHandle(texture.Image, texture.Description);
    VkDescriptorSet imageDescriptorSet = ImGui_ImplVulkan_AddTexture(Resources()[sampler].Sampler,
        Resources()[texture.Image].Views.ViewList[viewHandle.m_Index],
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
    vmaBuildStatsString(s_State.Allocator, &statsString, DETAILED_MAP);
    if (!exists(path.parent_path()))
        create_directories(path.parent_path());
    std::ofstream out(path);
    std::print(out, "{}", statsString);
    vmaFreeStatsString(s_State.Allocator, statsString);
}

void Device::BeginCommandBufferLabel(CommandBuffer cmd, std::string_view label)
{
#ifdef VULKAN_VAL_LAYERS
    VkDebugUtilsLabelEXT debugLabel = {};
    debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debugLabel.pLabelName = label.data();
    vkCmdBeginDebugUtilsLabelEXT(Resources()[cmd].CommandBuffer, &debugLabel);
#endif
}

void Device::EndCommandBufferLabel(CommandBuffer cmd)
{
#ifdef VULKAN_VAL_LAYERS
    vkCmdEndDebugUtilsLabelEXT(Resources()[cmd].CommandBuffer);
#endif
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
    nameVulkanHandle(s_State.Device, (u64)Resources()[buffer].Buffer, VK_OBJECT_TYPE_BUFFER, name);
}

void Device::NameImage(Image image, std::string_view name)
{
    nameVulkanHandle(s_State.Device, (u64)Resources()[image].Image, VK_OBJECT_TYPE_IMAGE, name);
}

void Device::NamePipeline(Pipeline pipeline, std::string_view name)
{
    nameVulkanHandle(s_State.Device, (u64)Resources()[pipeline].Pipeline, VK_OBJECT_TYPE_PIPELINE, name);
}

void Device::CompileCommand(CommandBuffer cmd, const ExecuteSecondaryBufferCommand& command)
{
    vkCmdExecuteCommands(Resources()[cmd].CommandBuffer, 1, &Resources()[command.Cmd].CommandBuffer);
}

void Device::CompileCommand(CommandBuffer cmd, const PrepareSwapchainPresentCommand& command)
{
    DeviceResources::SwapchainResource& swapchainResource = Resources()[command.Swapchain];
    
    ImageSubresource drawSubresource = {
        .Image = swapchainResource.Description.DrawImage,
        .Description = {.Mipmaps = 1, .Layers = 1}};
    ImageSubresource presentSubresource = {
        .Image = swapchainResource.Description.ColorImages[command.ImageIndex],
        .Description = {.Mipmaps = 1, .Layers = 1}};
    ::DeletionQueue deletionQueue = {};

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
        .DependencyInfo = CreateDependencyInfo({
        .LayoutTransitionInfo = presentToDestinationTransitionInfo}, deletionQueue)});

    ImageSubregion sourceSubregion = {
        .Mipmap = (u32)drawSubresource.Description.MipmapBase,
        .LayerBase = (u32)drawSubresource.Description.LayerBase,
        .Layers = (u32)drawSubresource.Description.Layers,
        .Top = GetImageDescription(swapchainResource.Description.DrawImage).Dimensions()};
    
    ImageSubregion destinationSubregion = {
        .Mipmap = (u32)presentSubresource.Description.MipmapBase,
        .LayerBase = (u32)presentSubresource.Description.LayerBase,
        .Layers = (u32)presentSubresource.Description.Layers,
        .Top = GetImageDescription(swapchainResource.Description.ColorImages[command.ImageIndex]).Dimensions()};

    CompileCommand(cmd, BlitImageCommand{
        .Source = swapchainResource.Description.DrawImage,
        .Destination = swapchainResource.Description.ColorImages[command.ImageIndex],
        .Filter = ImageFilter::Linear,
        .SourceSubregion = sourceSubregion,
        .DestinationSubregion = destinationSubregion});

    CompileCommand(cmd, WaitOnBarrierCommand{
        .DependencyInfo = CreateDependencyInfo({
            .LayoutTransitionInfo = destinationToPresentTransitionInfo}, deletionQueue)});
}

void Device::CompileCommand(CommandBuffer cmd, const BeginRenderingCommand& command)
{
    const DeviceResources::RenderingInfoResource& renderingInfoResource = Resources()[command.RenderingInfo];
    
    VkRenderingInfo renderingInfoVulkan = {};
    renderingInfoVulkan.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfoVulkan.layerCount = 1;
    renderingInfoVulkan.renderArea = VkRect2D{
        .offset = {},
        .extent = {renderingInfoResource.RenderArea.x, renderingInfoResource.RenderArea.y}};
    renderingInfoVulkan.colorAttachmentCount = (u32)Resources()[command.RenderingInfo].ColorAttachments.size();
    renderingInfoVulkan.pColorAttachments = Resources()[command.RenderingInfo].ColorAttachments.data();
    if (Resources()[command.RenderingInfo].DepthAttachment.has_value())
        renderingInfoVulkan.pDepthAttachment = Resources()[command.RenderingInfo].DepthAttachment.operator->();
    
    vkCmdBeginRendering(Resources()[cmd].CommandBuffer, &renderingInfoVulkan);
}

void Device::CompileCommand(CommandBuffer cmd, const EndRenderingCommand&)
{
    vkCmdEndRendering(Resources()[cmd].CommandBuffer);
}

void Device::CompileCommand(CommandBuffer cmd, const ImGuiBeginCommand& command)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Device::CompileCommand(CommandBuffer cmd, const ImGuiEndCommand& command)
{
    ImGui::Render();
    CompileCommand(cmd, BeginRenderingCommand{
        .RenderingInfo = command.RenderingInfo});
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Resources()[cmd].CommandBuffer);
    CompileCommand(cmd, EndRenderingCommand{});
}

void Device::CompileCommand(CommandBuffer cmd, const BeginConditionalRenderingCommand& command)
{
    VkConditionalRenderingBeginInfoEXT beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.buffer = Resources()[command.Buffer].Buffer;
    beginInfo.offset = command.Offset;

    vkCmdBeginConditionalRenderingEXT(Resources()[cmd].CommandBuffer, &beginInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const EndConditionalRenderingCommand& command)
{
    vkCmdEndConditionalRenderingEXT(Resources()[cmd].CommandBuffer);
}

void Device::CompileCommand(CommandBuffer cmd, const SetViewportCommand& command)
{
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)command.Size.x, .height = (f32)command.Size.y,
        .minDepth = 0.0f, .maxDepth = 1.0f};

    vkCmdSetViewport(Resources()[cmd].CommandBuffer, 0, 1, &viewport);   
}

void Device::CompileCommand(CommandBuffer cmd, const SetScissorsCommand& command)
{
    VkRect2D scissor = {
        .offset = {(i32)command.Offset.x, (i32)command.Offset.y},
        .extent = {(u32)command.Size.x, (u32)command.Size.y}};
    
    vkCmdSetScissor(Resources()[cmd].CommandBuffer, 0, 1, &scissor);
}

void Device::CompileCommand(CommandBuffer cmd, const SetDepthBiasCommand& command)
{
    vkCmdSetDepthBias(Resources()[cmd].CommandBuffer, command.Constant, 0.0f, command.Slope);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyBufferCommand& command)
{
    VkBufferCopy2 copy = {};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy.size = command.SizeBytes;
    copy.srcOffset = command.SourceOffset;
    copy.dstOffset = command.DestinationOffset;

    const DeviceResources::BufferResource& sourceResource = Resources()[command.Source];
    const DeviceResources::BufferResource& destinationResource = Resources()[command.Destination];
    VkCopyBufferInfo2 copyBufferInfo = {};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = sourceResource.Buffer;
    copyBufferInfo.dstBuffer = destinationResource.Buffer;
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copy;

    vkCmdCopyBuffer2(Resources()[cmd].CommandBuffer, &copyBufferInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyBufferToImageCommand& command)
{
    ASSERT(command.ImageSubresource.Mipmaps == 1, "Buffer to image copies one mipmap at a time")
    
    const DeviceResources::ImageResource& imageResource = Resources()[command.Image];
    
    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.imageExtent = {
        .width = imageResource.Description.Width,
        .height = imageResource.Description.Height,
        .depth =  imageResource.Description.GetDepth()};
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(imageResource.Description.Usage);
    bufferImageCopy.imageSubresource.mipLevel = (u32)(i32)command.ImageSubresource.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = (u32)(i32)command.ImageSubresource.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = (u32)(i32)command.ImageSubresource.Layers;
    
    const DeviceResources::BufferResource& sourceResource = Resources()[command.Buffer];
    VkCopyBufferToImageInfo2 copyBufferToImageInfo = {};
    copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copyBufferToImageInfo.srcBuffer = sourceResource.Buffer;
    copyBufferToImageInfo.dstImage = Resources()[command.Image].Image;
    copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyBufferToImageInfo.regionCount = 1;
    copyBufferToImageInfo.pRegions = &bufferImageCopy;

    vkCmdCopyBufferToImage2(Resources()[cmd].CommandBuffer, &copyBufferToImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const CopyImageCommand& command)
{
    const DeviceResources::ImageResource& sourceResource = Resources()[command.Source];
    const DeviceResources::ImageResource& destinationResource = Resources()[command.Destination];
    
    glm::uvec3 extentSource = command.SourceSubregion.Top - command.SourceSubregion.Bottom;
    glm::uvec3 extentDestination = command.DestinationSubregion.Top - command.DestinationSubregion.Bottom;
    ASSERT(extentSource == extentDestination, "Extents of source and destination must match for image copy")

    VkImageCopy2 imageCopy = {};
    VkCopyImageInfo2 copyImageInfo = {};
    
    imageCopy.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
    imageCopy.extent = VkExtent3D{
        .width = extentSource.x,
        .height = extentSource.y,
        .depth = extentSource.z};
    imageCopy.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(sourceResource.Description.Usage);
    imageCopy.srcSubresource.baseArrayLayer = command.SourceSubregion.LayerBase;
    imageCopy.srcSubresource.layerCount = command.SourceSubregion.Layers;
    imageCopy.srcSubresource.mipLevel = command.SourceSubregion.Mipmap;
    imageCopy.srcOffset = VkOffset3D{
        .x = (i32)command.SourceSubregion.Bottom.x,
        .y = (i32)command.SourceSubregion.Bottom.y,
        .z = (i32)command.SourceSubregion.Bottom.z};
    imageCopy.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destinationResource.Description.Usage);
    imageCopy.dstSubresource.baseArrayLayer = command.DestinationSubregion.LayerBase;
    imageCopy.dstSubresource.layerCount = command.DestinationSubregion.Layers;
    imageCopy.dstSubresource.mipLevel = command.DestinationSubregion.Mipmap;
    imageCopy.dstOffset = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Bottom.x,
        .y = (i32)command.DestinationSubregion.Bottom.y,
        .z = (i32)command.DestinationSubregion.Bottom.z};
    
    copyImageInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
    copyImageInfo.srcImage = sourceResource.Image;
    copyImageInfo.dstImage = destinationResource.Image;
    copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImageInfo.regionCount = 1;
    
    copyImageInfo.pRegions = &imageCopy;

    vkCmdCopyImage2(Resources()[cmd].CommandBuffer, &copyImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const BlitImageCommand& command)
{
    const DeviceResources::ImageResource& sourceResource = Resources()[command.Source];
    const DeviceResources::ImageResource& destinationResource = Resources()[command.Destination];

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
        .z = (i32)command.SourceSubregion.Bottom.z};
    imageBlit.srcOffsets[1] = VkOffset3D{
        .x = (i32)command.SourceSubregion.Top.x,
        .y = (i32)command.SourceSubregion.Top.y,
        .z = (i32)command.SourceSubregion.Top.z};

    imageBlit.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destinationResource.Description.Usage);
    imageBlit.dstSubresource.baseArrayLayer = command.DestinationSubregion.LayerBase;
    imageBlit.dstSubresource.layerCount = command.DestinationSubregion.Layers;
    imageBlit.dstSubresource.mipLevel = command.DestinationSubregion.Mipmap;
    imageBlit.dstOffsets[0] = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Bottom.x,
        .y = (i32)command.DestinationSubregion.Bottom.y,
        .z = (i32)command.DestinationSubregion.Bottom.z};
    imageBlit.dstOffsets[1] = VkOffset3D{
        .x = (i32)command.DestinationSubregion.Top.x,
        .y = (i32)command.DestinationSubregion.Top.y,
        .z = (i32)command.DestinationSubregion.Top.z};
    
    blitImageInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitImageInfo.srcImage = sourceResource.Image;
    blitImageInfo.dstImage = destinationResource.Image;
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.filter = vulkanFilterFromImageFilter(command.Filter);

    blitImageInfo.pRegions = &imageBlit;
    
    vkCmdBlitImage2(Resources()[cmd].CommandBuffer, &blitImageInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnFullPipelineBarrierCommand&)
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

    vkCmdPipelineBarrier2(Resources()[cmd].CommandBuffer, &dependencyInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnBarrierCommand& command)
{
    const DeviceResources::DependencyInfoResource& dependencyInfo = Resources()[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) : nullptr;
    vkCmdPipelineBarrier2(Resources()[cmd].CommandBuffer, &vkDependencyInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const SignalSplitBarrierCommand& command)
{
    const DeviceResources::DependencyInfoResource& dependencyInfo = Resources()[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) : nullptr;
    vkCmdSetEvent2(Resources()[cmd].CommandBuffer, Resources()[command.SplitBarrier].Event, &vkDependencyInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const WaitOnSplitBarrierCommand& command)
{
    const DeviceResources::DependencyInfoResource& dependencyInfo = Resources()[command.DependencyInfo];

    VkDependencyInfo vkDependencyInfo = dependencyInfo.DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = dependencyInfo.MemoryBarriersCount;
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.MemoryBarriers.data();
    vkDependencyInfo.imageMemoryBarrierCount = dependencyInfo.LayoutDependency ? 1u : 0u;
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.LayoutDependency ?
        std::to_address(dependencyInfo.LayoutDependency) : nullptr;
    vkCmdWaitEvents2(Resources()[cmd].CommandBuffer, 1, &Resources()[command.SplitBarrier].Event,
        &vkDependencyInfo);
}

void Device::CompileCommand(CommandBuffer cmd, const ResetSplitBarrierCommand& command)
{
    ASSERT(Resources()[command.DependencyInfo].MemoryBarriersCount != 0, "Invalid reset operation")
    vkCmdResetEvent2(Resources()[cmd].CommandBuffer, Resources()[command.SplitBarrier].Event,
        Resources()[command.DependencyInfo].MemoryBarriers.front().dstStageMask);
}

void Device::CompileCommand(CommandBuffer cmd, const BindVertexBuffersCommand& command)
{
    std::vector<VkBuffer> vkBuffers(command.Buffers.size());
    for (u32 i = 0; i < vkBuffers.size(); i++)
        vkBuffers[i] = Resources()[command.Buffers[i]].Buffer;
    
    vkCmdBindVertexBuffers(Resources()[cmd].CommandBuffer, 0, (u32)vkBuffers.size(), vkBuffers.data(),
        command.Offsets.data());
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU32BufferCommand& command)
{
    vkCmdBindIndexBuffer(Resources()[cmd].CommandBuffer, Resources()[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT32);
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU16BufferCommand& command)
{
    vkCmdBindIndexBuffer(Resources()[cmd].CommandBuffer, Resources()[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT16);
}

void Device::CompileCommand(CommandBuffer cmd, const BindIndexU8BufferCommand& command)
{
    vkCmdBindIndexBuffer(Resources()[cmd].CommandBuffer, Resources()[command.Buffer].Buffer, command.Offset,
        VK_INDEX_TYPE_UINT8_EXT);
}

void Device::CompileCommand(CommandBuffer cmd, const BindPipelineGraphicsCommand& command)
{
    vkCmdBindPipeline(Resources()[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        Resources()[command.Pipeline].Pipeline);
}

void Device::CompileCommand(CommandBuffer cmd, const BindPipelineComputeCommand& command)
{
    vkCmdBindPipeline(Resources()[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, 
        Resources()[command.Pipeline].Pipeline);
}

#ifdef DESCRIPTOR_BUFFER
void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersGraphicsCommand& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, Resources()[command.PipelineLayout].Layout, command.Set);
}

void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersComputeCommand& command)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, Resources()[command.PipelineLayout].Layout, command.Set);
}
#else
void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersGraphicsCommand& command)
{
    vkCmdBindDescriptorSets(Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, Resources()[command.PipelineLayout].Layout, command.Set, 1,
        &Resources()[command.Descriptors].DescriptorSet, 0, nullptr);
}

void Device::CompileCommand(CommandBuffer cmd, const BindImmutableSamplersComputeCommand& command)
{
    vkCmdBindDescriptorSets(Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, Resources()[command.PipelineLayout].Layout, command.Set, 1,
        &Resources()[command.Descriptors].DescriptorSet, 0, nullptr);
}
#endif

void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorsGraphicsCommand& command)
{
    DeviceInternal::BindDescriptors(cmd, *command.Allocators, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorsComputeCommand& command)
{
    DeviceInternal::BindDescriptors(cmd, *command.Allocators, command.PipelineLayout, command.Descriptors, command.Set,
        VK_PIPELINE_BIND_POINT_COMPUTE);
}

#ifdef DESCRIPTOR_BUFFER
void Device::CompileCommand(CommandBuffer cmd, const BindDescriptorArenaAllocatorsCommand& command)
{
    std::vector<VkDescriptorBufferBindingInfoEXT> descriptorBufferBindings;
    descriptorBufferBindings.reserve(command.Allocators->m_Allocators.size());

    for (auto& allocator : command.Allocators->m_Allocators)
    {
        const DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Resources()[allocator];
        const u64 deviceAddress = allocatorResource.DeviceAddress;

        VkDescriptorBufferBindingInfoEXT binding = {};
        binding.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        binding.address = deviceAddress;
        binding.usage = allocatorResource.DescriptorBufferUsage;

        descriptorBufferBindings.push_back(binding);
    }

    vkCmdBindDescriptorBuffersEXT(Resources()[cmd].CommandBuffer, (u32)descriptorBufferBindings.size(),
        descriptorBufferBindings.data());
}
#else
void Device::CompileCommand(CommandBuffer, const BindDescriptorArenaAllocatorsCommand&)
{
}
#endif

void Device::CompileCommand(CommandBuffer cmd, const PushConstantsCommand& command)
{
    const DeviceResources::PipelineLayoutResource& layout = Resources()[command.PipelineLayout];
    const VkPushConstantRange& pushConstantRange = layout.PushConstants.front();
    vkCmdPushConstants(Resources()[cmd].CommandBuffer, layout.Layout,
        pushConstantRange.stageFlags, 0, pushConstantRange.size, command.Data.data());
}

void Device::CompileCommand(CommandBuffer cmd, const DrawCommand& command)
{
    vkCmdDraw(Resources()[cmd].CommandBuffer, command.VertexCount, 1, 0, command.BaseInstance);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedCommand& command)
{
    vkCmdDrawIndexed(Resources()[cmd].CommandBuffer, command.IndexCount, 1, 0, 0, command.BaseInstance);
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCommand& command)
{
    vkCmdDrawIndexedIndirect(Resources()[cmd].CommandBuffer, Resources()[command.Buffer].Buffer,
        command.Offset, command.Count, command.Stride);   
}

void Device::CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCountCommand& command)
{
    vkCmdDrawIndexedIndirectCount(Resources()[cmd].CommandBuffer,
        Resources()[command.DrawBuffer].Buffer, command.DrawOffset,
        Resources()[command.CountBuffer].Buffer, command.CountOffset,
        command.MaxCount, command.Stride);
}

void Device::CompileCommand(CommandBuffer cmd, const DispatchCommand& command)
{
    const glm::uvec3 groupSize = (command.Invocations + command.GroupSize - glm::uvec3{1}) / command.GroupSize;
    vkCmdDispatch(Resources()[cmd].CommandBuffer, groupSize.x, groupSize.y, groupSize.z);
}

void Device::CompileCommand(CommandBuffer cmd, const DispatchIndirectCommand& command)
{
    vkCmdDispatchIndirect(Resources()[cmd].CommandBuffer, Resources()[command.Buffer].Buffer, command.Offset);
}

#ifdef DESCRIPTOR_BUFFER
u32 DeviceInternal::GetDescriptorSizeBytes(DescriptorType type)
{
    auto& props = Device::s_State.GPUDescriptorBufferProperties;
    switch (type)
    {
    case DescriptorType::Sampler:       return (u32)props.samplerDescriptorSize;
    case DescriptorType::Image:         return (u32)props.sampledImageDescriptorSize;
    case DescriptorType::ImageStorage:  return (u32)props.storageImageDescriptorSize;
    case DescriptorType::TexelUniform:  return (u32)props.uniformTexelBufferDescriptorSize;
    case DescriptorType::TexelStorage:  return (u32)props.storageTexelBufferDescriptorSize;
    case DescriptorType::UniformBuffer: return (u32)props.uniformBufferDescriptorSize;
    case DescriptorType::StorageBuffer: return (u32)props.storageBufferDescriptorSize;
    case DescriptorType::Input:         return (u32)props.inputAttachmentDescriptorSize;
    default:
        return 0;
    }
}

void DeviceInternal::WriteDescriptor(Descriptors descriptors, DescriptorSlotInfo slotInfo, u32 index,
    VkDescriptorGetInfoEXT& descriptorGetInfo)
{
    auto&& [slot, type] = slotInfo;
    const DeviceResources::DescriptorsResource& descriptorsResource = Device::Resources()[descriptors];
    const u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    const u64 innerOffsetBytes = descriptorSizeBytes * index;
    ASSERT(innerOffsetBytes + descriptorSizeBytes <= descriptorsResource.SizeBytes,
        "Trying to write descriptor outside of the allocated region")

    const u64 offsetBytes = descriptorsResource.Offsets[slot] + innerOffsetBytes;
    const DeviceResources::DescriptorArenaAllocatorResource& allocatorResource =
        Device::Resources()[descriptorsResource.Allocator];
    vkGetDescriptorEXT(Device::s_State.Device, &descriptorGetInfo, descriptorSizeBytes,
        (u8*)allocatorResource.MappedAddress + offsetBytes);
}
void DeviceInternal::BindDescriptors(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    const DeviceResources::DescriptorsResource& descriptorsResource = Device::Resources()[descriptors];
    const DeviceResources::DescriptorArenaAllocatorResource& allocatorResource =
        Device::Resources()[descriptorsResource.Allocator];
    ASSERT(allocators.Get(allocatorResource.DescriptorSet) == descriptorsResource.Allocator,
        "Descriptors were not allocated by any of the provided allocators")

    u64 offset = descriptorsResource.Offsets.front();
    vkCmdSetDescriptorBufferOffsetsEXT(Device::Resources()[cmd].CommandBuffer, bindPoint,
        Device::Resources()[pipelineLayout].Layout, firstSet, 1, &allocatorResource.DescriptorSet, &offset);
}
#else
u32 DeviceInternal::GetFreePoolIndexFromAllocator(DescriptorArenaAllocator allocator, DescriptorPoolFlags poolFlags)
{
    DeviceResources::DescriptorArenaAllocatorResource& allocatorResource = Device::Resources()[allocator];
    for (u32 i = 0; i < allocatorResource.FreePools.size(); i++)
        if (allocatorResource.FreePools[i].Flags == poolFlags)
            return i;

    u32 index = (u32)allocatorResource.FreePools.size();
    std::vector<VkDescriptorPoolSize> sizes(allocatorResource.PoolSizes.size());
    for (u32 i = 0; i < sizes.size(); i++)
        sizes[i] = {
        .type = vulkanDescriptorTypeFromDescriptorType(allocatorResource.PoolSizes[i].DescriptorType),
        .descriptorCount =
            (u32)(allocatorResource.PoolSizes[i].SetSizeMultiplier * (f32)allocatorResource.MaxSetsPerPool)};

    VkDescriptorPool pool = {};
    
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = allocatorResource.MaxSetsPerPool;
    poolCreateInfo.poolSizeCount = (u32)sizes.size();
    poolCreateInfo.pPoolSizes = sizes.data();
    poolCreateInfo.flags = vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(poolFlags);

    deviceCheck(vkCreateDescriptorPool(Device::s_State.Device, &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    allocatorResource.FreePools.push_back({.Pool = pool, .Flags = poolFlags});

    return index;
}
void DeviceInternal::BindDescriptors(CommandBuffer cmd, const DescriptorArenaAllocators&,
    PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    const DeviceResources::DescriptorsResource& descriptorsResource = Device::Resources()[descriptors];
    vkCmdBindDescriptorSets(Device::Resources()[cmd].CommandBuffer, bindPoint,
        Device::Resources()[pipelineLayout].Layout, firstSet, 1, &descriptorsResource.DescriptorSet, 0, nullptr);
}
#endif

VmaAllocator& DeviceInternal::Allocator()
{
    return Device::s_State.Allocator;
}

Buffer DeviceInternal::AllocateBuffer(const BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags allocationFlags)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = createInfo.SizeBytes;
    bufferCreateInfo.usage = usage;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = allocationFlags;

    DeviceResources::BufferResource bufferResource = {};
    deviceCheck(vmaCreateBuffer(Allocator(), &bufferCreateInfo, &allocationCreateInfo,
        &bufferResource.Buffer, &bufferResource.Allocation, nullptr),
        "Failed to create a buffer");

    bufferResource.Description.SizeBytes = createInfo.SizeBytes;
    bufferResource.Description.Usage = createInfo.Usage;
    if (createInfo.PersistentMapping)
        bufferResource.HostAddress = bufferResource.Allocation->GetMappedData();

    return Device::Resources().AddResource(bufferResource);
}

VkImageView DeviceInternal::CreateVulkanImageView(const ImageSubresource& image, VkFormat format)
{
    const DeviceResources::ImageResource& imageResource = Device::Resources()[image.Image];
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = Device::Resources()[image.Image].Image;
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

    deviceCheck(vkCreateImageView(Device::s_State.Device, &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

std::vector<VkSemaphoreSubmitInfo> DeviceInternal::CreateVulkanSemaphoreSubmit(Span<const Semaphore> semaphores,
    Span<const PipelineStage> waitStages)
{
    std::vector waitSemaphoreSubmitInfos(semaphores.size(), VkSemaphoreSubmitInfo{});
    for (auto&& [i, semaphore] : std::views::enumerate(semaphores))
    {
        waitSemaphoreSubmitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreSubmitInfos[i].semaphore = Device::Resources()[semaphore].Semaphore;
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
        waitSemaphoreSubmitInfos[i].semaphore = Device::Resources()[semaphore].Semaphore;
        waitSemaphoreSubmitInfos[i].value = waitValues[i];
        waitSemaphoreSubmitInfos[i].stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i]);
    }
    
    return waitSemaphoreSubmitInfos;
}