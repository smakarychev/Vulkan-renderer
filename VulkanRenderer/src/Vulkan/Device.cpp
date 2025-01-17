#include "Device.h"

#include "Core/core.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <imgui/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <imgui/imgui_impl_glfw.h>
#include <fstream>
#include <print>

#include "AssetManager.h"
#include "RenderCommand.h"
#include "ResourceUploader.h"
#include "TextureAsset.h"
#include "utils/CoreUtils.h"
#include "Rendering/Buffer.h"
#include "Core/ProfilerContext.h"
#include "Rendering/FormatTraits.h"
#include "utils/utils.h"

#include "Imgui/ImguiUI.h"

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

        std::vector<std::pair<ImageUsage, VkImageUsageFlags>> MAPPINGS {
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
        const std::vector<std::pair<PipelineStage, VkPipelineStageFlags2>> MAPPINGS {
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
        const std::vector<std::pair<PipelineAccess, VkAccessFlagBits2>> MAPPINGS {
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
        const std::vector<std::pair<PipelineDependencyFlags, VkDependencyFlags>> MAPPINGS {
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

    constexpr VkDescriptorPoolCreateFlags vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(DescriptorPoolFlags flags)
    {
        VkDescriptorPoolCreateFlags poolFlags = 0;
        if (enumHasAny(flags, DescriptorPoolFlags::UpdateAfterBind))
            poolFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        if (enumHasAny(flags, DescriptorPoolFlags::HostOnly))
            poolFlags |= VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT;

        return poolFlags;
    }

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

    Sampler getImmutableSampler(ImageFilter filter, SamplerWrapMode wrapMode, SamplerBorderColor borderColor)
    {
        Sampler sampler = Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter,
            .WrapMode = wrapMode,
            .BorderColor = borderColor});

        return sampler;
    }
    Sampler getImmutableShadowSampler(ImageFilter filter, SamplerDepthCompareMode depthCompareMode)
    {
        Sampler sampler = Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter,
            .WrapMode = SamplerWrapMode::ClampBorder,
            .BorderColor = SamplerBorderColor::Black,
            .DepthCompareMode = depthCompareMode,
            .WithAnisotropy = false});

        return sampler;
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
}

void DeletionQueue::Flush()
{
    for (auto handle : m_Buffers)
        Device::Destroy(handle);
    for (auto handle : m_Images)
        Device::Destroy(handle);
    for (auto handle : m_Samplers)
        Device::Destroy(handle);
    
    for (auto handle : m_CommandPools)
        Device::Destroy(handle);
    
    for (auto handle : m_DescriptorLayouts)
        Device::Destroy(handle);
    for (auto handle : m_DescriptorAllocators)
        Device::Destroy(handle);
    
    for (auto handle : m_PipelineLayouts)
        Device::Destroy(handle);
    for (auto handle : m_Pipelines)
        Device::Destroy(handle);
    for (auto handle : m_ShaderModules)
        Device::Destroy(handle);
    
    for (auto handle : m_RenderingAttachments)
        Device::Destroy(handle);
    for (auto handle : m_RenderingInfos)
        Device::Destroy(handle);
    
    for (auto handle : m_Fences)
        Device::Destroy(handle);
    for (auto handle : m_Semaphores)
        Device::Destroy(handle);
    for (auto handle : m_TimelineSemaphore)
        Device::Destroy(handle);
    for (auto handle : m_DependencyInfos)
        Device::Destroy(handle);
    for (auto handle : m_SplitBarriers)
        Device::Destroy(handle);
    
    for (auto handle : m_Queues)
        Device::Destroy(handle);
    
    for (auto handle : m_Swapchains)
        Device::Destroy(handle);
    
    m_Swapchains.clear();
    m_Buffers.clear();
    m_Images.clear();
    m_Samplers.clear();
    m_CommandPools.clear();
    m_Queues.clear();
    m_DescriptorLayouts.clear();
    m_DescriptorAllocators.clear();
    m_PipelineLayouts.clear();
    m_Pipelines.clear();
    m_ShaderModules.clear();
    m_RenderingAttachments.clear();
    m_RenderingInfos.clear();
    m_Fences.clear();
    m_Semaphores.clear();
    m_DependencyInfos.clear();
    m_SplitBarriers.clear();
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
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    createInfo.Window = window;
    createInfo.AsyncCompute = asyncCompute;

    return createInfo;
}

void DeviceResources::MapCmdToPool(const CommandBuffer& cmd, const CommandPool& pool)
{
    m_CommandPoolToBuffersMap[pool.Handle().m_Id].push_back(cmd.Handle().m_Id);
}

void DeviceResources::DestroyCmdsOfPool(ResourceHandleType<CommandPool> pool)
{
    for (auto index : m_CommandPoolToBuffersMap[pool.m_Id])
        m_CommandBuffers.Remove(index);
    m_DeallocatedCount += (u32)m_CommandPoolToBuffersMap[pool.m_Id].size(); 
    m_CommandPoolToBuffersMap[pool.m_Id].clear();
}


void DeviceResources::MapDescriptorSetToAllocator(const DescriptorSet& set, const DescriptorAllocator& allocator)
{
    m_DescriptorAllocatorToSetsMap[allocator.Handle().m_Id].push_back(set.Handle().m_Id);
}

void DeviceResources::DestroyDescriptorSetsOfAllocator(ResourceHandleType<DescriptorAllocator> allocator)
{
    for (auto index : m_DescriptorAllocatorToSetsMap[allocator.m_Id])
        m_DescriptorSets.Remove(index);
    m_DeallocatedCount += (u32)m_DescriptorAllocatorToSetsMap[allocator.m_Id].size(); 
    m_DescriptorAllocatorToSetsMap[allocator.m_Id].clear();
}

struct Device::State
{
    VkDevice Device{VK_NULL_HANDLE};
    DeviceResources Resources;
    VmaAllocator Allocator;
    DeviceQueues Queues;
    ::DeletionQueue DeletionQueue;
    ::DeletionQueue DummyDeletionQueue;
    
    GLFWwindow* Window{nullptr};
    
    ImmediateSubmitContext SubmitContext;

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

Device::State Device::s_State = State{};

void Device::Destroy(ResourceHandleType<QueueInfo> queue)
{
    Resources().RemoveResource(queue);
}

Swapchain Device::CreateSwapchain(SwapchainCreateInfo&& createInfo)
{
    std::vector<VkSurfaceFormatKHR> desiredFormats = {{{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    std::vector<VkPresentModeKHR> desiredPresentModes = {{
        //VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR}};
    
    SurfaceDetails surfaceDetails = getSurfaceDetails(s_State.GPU, s_State.Surface);
    VkSurfaceCapabilitiesKHR capabilities = surfaceDetails.Capabilities;
    VkSurfaceFormatKHR colorFormat = Utils::getIntersectionOrDefault(
        desiredFormats, surfaceDetails.Formats,
        [](VkSurfaceFormatKHR des, VkSurfaceFormatKHR avail)
        {
            return des.format == avail.format && des.colorSpace == avail.colorSpace;
        });
    VkPresentModeKHR presentMode = Utils::getIntersectionOrDefault(
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
    DeviceCheck(vkCreateSwapchainKHR(s_State.Device, &swapchainCreateInfo, nullptr, &swapchainResource.Swapchain),
        "Failed to create swapchain");
    
    Swapchain swapchain = {};
    swapchain.m_ResourceHandle = Resources().AddResource(swapchainResource);
    swapchain.m_SwapchainResolution = glm::uvec2{extent.width, extent.height};
    swapchain.m_DrawResolution = createInfo.DrawResolution.x != 0 ?
        createInfo.DrawResolution : swapchain.m_SwapchainResolution;
    swapchain.m_DrawFormat = createInfo.DrawFormat;
    swapchain.m_DepthFormat = createInfo.DepthStencilFormat;
    swapchain.m_ColorImages = swapchain.CreateColorImages();
    swapchain.m_DrawImage = swapchain.CreateDrawImage();
    swapchain.m_DepthImage = swapchain.CreateDepthImage();
    swapchain.m_ColorImageCount = imageCount;
    swapchain.m_SwapchainFrameSync.assign_range(createInfo.FrameSyncs);
    swapchain.m_Window = s_State.Window;

    if (swapchain.m_SwapchainFrameSync.empty())
    {
        swapchain.m_SwapchainFrameSync.reserve(BUFFERED_FRAMES);
        for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        {
            Fence renderFence = CreateFence({
                .IsSignaled = true});
            Semaphore renderSemaphore = CreateSemaphore();
            Semaphore presentSemaphore = CreateSemaphore();

            swapchain.m_SwapchainFrameSync.push_back({
                .RenderFence = renderFence,
                .RenderSemaphore = renderSemaphore,
                .PresentSemaphore = presentSemaphore});
        }
    }
    ASSERT(swapchain.m_SwapchainFrameSync.size() == BUFFERED_FRAMES,
        "Frame synchronization structures for swapchain have to be provided for every frame-in-flight ({})",
        BUFFERED_FRAMES)

    return swapchain;
}

void Device::Destroy(ResourceHandleType<Swapchain> swapchain)
{
    vkDestroySwapchainKHR(s_State.Device, Resources().m_Swapchains[swapchain.m_Id].Swapchain, nullptr);
    Resources().RemoveResource(swapchain);
}

std::vector<Image> Device::CreateSwapchainImages(const Swapchain& swapchain)
{
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(s_State.Device, Resources()[swapchain].Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(s_State.Device, Resources()[swapchain].Swapchain, &imageCount, images.data());

    ImageDescription description = {
        .Width = swapchain.m_SwapchainResolution.x,
        .Height = swapchain.m_SwapchainResolution.y,
        .LayersDepth = 1,
        .Mipmaps = 1,
        .Kind = ImageKind::Image2d,
        .Usage = ImageUsage::Destination};
    std::vector<Image> colorImages(imageCount);
    for (auto& image : colorImages)
        image.m_Description = description;
    
    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        DeviceResources::ImageResource imageResource = {.Image = images[i]};
        colorImages[i].m_ResourceHandle = Resources().AddResource(imageResource);
        Resources()[colorImages[i]].Views.ViewType.View = CreateVulkanImageView(
            ImageSubresource{.Image = &colorImages[i], .Description = {.Mipmaps = 1, .Layers = 1}},
            Resources()[swapchain].ColorFormat);
        Resources()[colorImages[i]].Views.ViewList = &Resources()[colorImages[i]].Views.ViewType.View;
    }

    return colorImages;
}

void Device::DestroySwapchainImages(const Swapchain& swapchain)
{
    for (const auto& colorImage : swapchain.m_ColorImages)
    {
        vkDestroyImageView(s_State.Device, *Resources()[colorImage].Views.ViewList, nullptr);
        Resources().RemoveResource(colorImage.Handle());
    }
    Image::Destroy(swapchain.m_DrawImage);
    Image::Destroy(swapchain.m_DepthImage);
}

u32 Device::AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync)
{
    WaitForFence(swapchainFrameSync.RenderFence);

    u32 imageIndex;
    VkResult res = vkAcquireNextImageKHR(s_State.Device, Resources()[swapchain].Swapchain,
        10'000'000'000, Resources()[swapchainFrameSync.PresentSemaphore].Semaphore, VK_NULL_HANDLE,
        &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        return INVALID_SWAPCHAIN_IMAGE;
    
    ASSERT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image")

    ResetFence(swapchainFrameSync.RenderFence);
    
    return imageIndex;
}

bool Device::Present(const Swapchain& swapchain, QueueKind queueKind, const SwapchainFrameSync& swapchainFrameSync,
    u32 imageIndex)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &Resources()[swapchain].Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &Resources()[swapchainFrameSync.RenderSemaphore].Semaphore;

    VkResult result = vkQueuePresentKHR(Resources()[s_State.Queues.GetQueueByKind(queueKind)].Queue, &presentInfo);
    
    ASSERT(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR,
        "Failed to present image")

    return result == VK_SUCCESS;
}

CommandBuffer Device::CreateCommandBuffer(CommandBufferCreateInfo&& createInfo)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = Resources()[*createInfo.Pool].CommandPool;
    allocateInfo.level = vulkanBufferLevelFromBufferKind(createInfo.Kind);
    allocateInfo.commandBufferCount = 1;

    DeviceResources::CommandBufferResource commandBufferResource = {};
    DeviceCheck(vkAllocateCommandBuffers(s_State.Device, &allocateInfo, &commandBufferResource.CommandBuffer),
        "Failed to allocate command buffer");
    
    CommandBuffer cmd = {};
    cmd.m_Kind = createInfo.Kind;
    cmd.m_ResourceHandle = Resources().AddResource(commandBufferResource);
    Resources().MapCmdToPool(cmd, *createInfo.Pool);
    
    return cmd;
}

CommandPool Device::CreateCommandPool(CommandPoolCreateInfo&& createInfo)
{
    VkCommandPoolCreateFlags flags = 0;
    if (createInfo.PerBufferReset)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = flags;
    poolCreateInfo.queueFamilyIndex = s_State.Queues.GetFamilyByKind(createInfo.QueueKind);

    DeviceResources::CommandPoolResource commandPoolResource = {};
    DeviceCheck(vkCreateCommandPool(s_State.Device, &poolCreateInfo, nullptr, &commandPoolResource.CommandPool),
        "Failed to create command pool");
    
    CommandPool commandPool = {};
    commandPool.m_ResourceHandle = Resources().AddResource(commandPoolResource);
    if (commandPool.Handle().m_Id >= Resources().m_CommandPoolToBuffersMap.size())
        Resources().m_CommandPoolToBuffersMap.resize(commandPool.Handle().m_Id + 1);
    
    return commandPool;
}

void Device::Destroy(ResourceHandleType<CommandPool> commandPool)
{
    vkDestroyCommandPool(s_State.Device, Resources().m_CommandPools[commandPool.m_Id].CommandPool, nullptr);
    Resources().DestroyCmdsOfPool(commandPool);
    Resources().RemoveResource(commandPool);
}

void Device::ResetPool(const CommandPool& pool)
{
    DeviceCheck(vkResetCommandPool(s_State.Device, Resources()[pool].CommandPool, 0),
        "Error while resetting command pool");
}

void Device::ResetCommandBuffer(const CommandBuffer& cmd)
{
    DeviceCheck(vkResetCommandBuffer(Resources()[cmd].CommandBuffer, 0), "Error while resetting command buffer");
}

void Device::BeginCommandBuffer(const CommandBuffer& cmd, CommandBufferUsage usage)
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = vulkanCommandBufferFlagsFromUsage(usage);
    if (cmd.m_Kind == CommandBufferKind::Secondary)
        beginInfo.pInheritanceInfo = &inheritanceInfo;
    
    DeviceCheck(vkBeginCommandBuffer(Resources()[cmd].CommandBuffer, &beginInfo),
        "Error while beginning command buffer");
}

void Device::EndCommandBuffer(const CommandBuffer& cmd)
{
    DeviceCheck(vkEndCommandBuffer(Resources()[cmd].CommandBuffer), "Error while ending command buffer");
}

void Device::SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind, const BufferSubmitSyncInfo& submitSync)
{
    SubmitCommandBuffers({cmd}, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    SubmitCommandBuffers({cmd}, queueKind, submitSync);
}

void Device::SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind, Fence fence)
{
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo.commandBuffer = Resources()[cmd].CommandBuffer;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
    
    DeviceCheck(vkQueueSubmit2(Resources()[s_State.Queues.GetQueueByKind(queueKind)].Queue, 1, &submitInfo,
        fence.HasValue() ? Resources()[fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffer");
}

void Device::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, QueueKind queueKind,
    const BufferSubmitSyncInfo& submitSync)
{
    std::vector<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;
    commandBufferSubmitInfos.reserve(cmds.size());
    for (auto& cmd : cmds)
        commandBufferSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = Resources()[cmd].CommandBuffer});

    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    signalSemaphoreSubmitInfos.reserve(submitSync.SignalSemaphores.size());
    for (auto& semaphore : submitSync.SignalSemaphores)
        signalSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = Resources()[*semaphore].Semaphore});

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitStages);
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    DeviceCheck(vkQueueSubmit2(Resources()[s_State.Queues.GetQueueByKind(queueKind)].Queue, 1, &submitInfo,
        submitSync.Fence.HasValue() ? Resources()[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");
}

void Device::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, QueueKind queueKind,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        Resources()[*submitSync.SignalSemaphores[i]].Timeline = submitSync.SignalValues[i];

    std::vector<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;
    commandBufferSubmitInfos.reserve(cmds.size());
    for (auto& cmd : cmds)
        commandBufferSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = Resources()[cmd].CommandBuffer});

    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    signalSemaphoreSubmitInfos.reserve(submitSync.SignalSemaphores.size());
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        signalSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = Resources()[*submitSync.SignalSemaphores[i]].Semaphore,
            .value = submitSync.SignalValues[i]});

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos = CreateVulkanSemaphoreSubmit(
        submitSync.WaitSemaphores, submitSync.WaitValues, submitSync.WaitStages);
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    DeviceCheck(vkQueueSubmit2(Resources()[s_State.Queues.GetQueueByKind(queueKind)].Queue, 1, &submitInfo,
        submitSync.Fence.HasValue() ? Resources()[submitSync.Fence].Fence : VK_NULL_HANDLE),
        "Error while submitting command buffers");    
}

Buffer Device::CreateBuffer(BufferCreateInfo&& createInfo)
{
    VmaAllocationCreateFlags flags = 0;
    if (enumHasAny(createInfo.Usage, BufferUsage::Mappable))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(createInfo.Usage, BufferUsage::MappableRandomAccess))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    if (createInfo.PersistentMapping)
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    DeviceResources::BufferResource bufferResource = CreateBufferResource(createInfo.SizeBytes,
        vulkanBufferUsageFromUsage(createInfo.Usage), flags);

    Buffer buffer = {};
    buffer.m_Description.Usage = createInfo.Usage;
    buffer.m_Description.SizeBytes = createInfo.SizeBytes;
    if (createInfo.PersistentMapping)
        buffer.m_HostAddress = bufferResource.Allocation->GetMappedData();
    
    buffer.m_ResourceHandle = Resources().AddResource(bufferResource);    

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
            ImmediateSubmit([&](const CommandBuffer& cmd)
            {
                RenderCommand::CopyBuffer(cmd, stagingBuffer, buffer,
                    {.SizeBytes = createInfo.InitialData.size(), .SourceOffset = 0, .DestinationOffset = 0});        
            });
            Destroy(stagingBuffer.Handle());
        }
    }
    
    return buffer;
}

DeviceResources::BufferResource Device::CreateBufferResource(u64 sizeBytes, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags allocationFlags)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = sizeBytes;
    bufferCreateInfo.usage = usage;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = allocationFlags;

    DeviceResources::BufferResource bufferResource = {};
    DeviceCheck(vmaCreateBuffer(Allocator(), &bufferCreateInfo, &allocationCreateInfo,
        &bufferResource.Buffer, &bufferResource.Allocation, nullptr),
        "Failed to create a buffer");

    return bufferResource;
}

void Device::Destroy(ResourceHandleType<Buffer> buffer)
{
    const DeviceResources::BufferResource& resource = Resources().m_Buffers[buffer.m_Id];
    vmaDestroyBuffer(Allocator(), resource.Buffer, resource.Allocation);
    Resources().RemoveResource(buffer);
}

Buffer Device::CreateStagingBuffer(u64 sizeBytes)
{
    return CreateBuffer({
        .SizeBytes = sizeBytes,
        .Usage = BufferUsage::Staging | BufferUsage::Mappable,
        .PersistentMapping = true});
}

void* Device::MapBuffer(const Buffer& buffer)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    void* mappedData;
    vmaMapMemory(Allocator(), resource.Allocation, &mappedData);
    return mappedData;
}

void Device::UnmapBuffer(const Buffer& buffer)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    vmaUnmapMemory(Allocator(), resource.Allocation);
}

void Device::SetBufferData(Buffer& buffer, Span<const std::byte> data, u64 offsetBytes)
{
    const DeviceResources::BufferResource& resource = Resources()[buffer];
    vmaCopyMemoryToAllocation(Allocator(), data.data(), resource.Allocation, offsetBytes, data.size());
}

void Device::SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    mappedAddress = (void*)((u8*)mappedAddress + offsetBytes);
    std::memcpy(mappedAddress, data.data(), data.size());
}

u64 Device::GetDeviceAddress(const Buffer& buffer)
{
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = Resources()[buffer].Buffer;
    
    return vkGetBufferDeviceAddress(s_State.Device, &deviceAddressInfo);
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
        Image* image = AssetManager::GetImage(assetPath);
        if (image)
        {
            createInfo.Description.Usage |= ImageUsage::NoDeallocation;
            return *image;
        }
    }

    assetLib::File textureFile;
    assetLib::loadAssetFile(assetPath, textureFile);
    assetLib::TextureInfo textureInfo = assetLib::readTextureInfo(textureFile);

    Buffer imageBuffer = CreateBuffer({
        .SizeBytes = textureInfo.SizeBytes,
        .Usage = BufferUsage::Source | BufferUsage::StagingRandomAccess,
        .PersistentMapping = true});
    assetLib::unpackTexture(
        textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)imageBuffer.GetHostAddress());
                    
    createInfo.Description.Format = formatFromAssetFormat(textureInfo.Format);
    createInfo.Description.Width = textureInfo.Dimensions.Width;
    createInfo.Description.Height = textureInfo.Dimensions.Height;
    createInfo.Description.Mipmaps = Image::CalculateMipmapCount({
        textureInfo.Dimensions.Width, textureInfo.Dimensions.Height});
    // todo: not always correct, should reflect in in asset file
    createInfo.Description.Kind = ImageKind::Image2d;
    createInfo.Description.LayersDepth = 1;

    Image image = CreateImageFromBuffer(createInfo, imageBuffer);
    AssetManager::AddImage(assetPath, image);
    Destroy(imageBuffer.Handle());

    return image;
}

Image Device::CreateImageFromPixels(ImageCreateInfo& createInfo, Span<const std::byte> pixels)
{
    if (pixels.empty())
    {
        Image image = {};

        DeviceResources::ImageResource imageResource = CreateImageResource(createInfo);
        image.m_ResourceHandle = Resources().AddResource(imageResource);
        // todo: remove once handles are ready
        image.m_Description = createInfo.Description;    
        CreateViews(ImageSubresource{.Image = &image}, createInfo.Description.AdditionalViews);
        
        return image;
    }
    
    Buffer imageBuffer = CreateBuffer({
        .SizeBytes = pixels.size(),
        .Usage = BufferUsage::Source | BufferUsage::Staging,
        .InitialData = pixels});

    Image image = CreateImageFromBuffer(createInfo, imageBuffer);
    Destroy(imageBuffer.Handle());

    return image;
}

Image Device::CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer)
{
    Image image = {};

    DeviceResources::ImageResource imageResource = CreateImageResource(createInfo);
    image.m_ResourceHandle = Resources().AddResource(imageResource);
    // todo: remove once handles are ready
    image.m_Description = createInfo.Description;
    CreateViews(ImageSubresource{.Image = &image}, createInfo.Description.AdditionalViews);
    
    ImageSubresource imageSubresource = {.Image = &image, .Description = {.Mipmaps = 1, .Layers = 1}};

    ImmediateSubmit([&](const CommandBuffer& cmd)
    {
        ::DeletionQueue deletionQueue = {};
        RenderCommand::WaitOnBarrier(cmd, CreateDependencyInfo({
            .LayoutTransitionInfo = LayoutTransitionInfo{
                .ImageSubresource = imageSubresource,
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllTransfer,
                .SourceAccess = PipelineAccess::None,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = ImageLayout::Undefined,
                .NewLayout = ImageLayout::Destination}},
            deletionQueue));

        RenderCommand::CopyBufferToImage(cmd, buffer,
            ImageSubresource{
                .Image = &image, .Description = {.Mipmaps = 1, .Layers = image.m_Description.GetLayers()}});
        if (createInfo.CalculateMipmaps)
            CalculateMipmaps(image, cmd, ImageLayout::Destination);
        imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
    });
    
    return image;
}

void Device::CalculateMipmaps(const Image& image, const CommandBuffer& cmd, ImageLayout currentLayout)
{
    if (image.Description().Mipmaps == 1)
        return;

    i32 width = (i32)image.Description().Width;
    i32 height = (i32)image.Description().Height;
    i32 depth = (i32)image.Description().GetDepth();
    i8 layers = image.Description().GetLayers();

    ::DeletionQueue deletionQueue = {};
    
    ImageSubresource imageSubresource = {
        .Image = &image,
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
    
    RenderCommand::WaitOnBarrier(cmd, CreateDependencyInfo({
        .LayoutTransitionInfo = transitionInfo}, deletionQueue));
    for (i8 mip = 1; mip < image.Description().Mipmaps; mip++)
    {
        ImageBlitInfo source = {
            .Image = &image,
            .MipmapBase = (u32)mip - 1,
            .Layers = (u32)layers,
            .Top = {width, height, depth}};

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
        depth = std::max(1, depth >> 1);

        ImageBlitInfo destination = {
            .Image = &image,
            .MipmapBase = (u32)mip,
            .LayerBase = 0,
            .Layers = (u32)layers,
            .Bottom = {},
            .Top = {width, height, depth}};

        ImageSubresource mipmapSubresource = {
            .Image = &image,
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
        RenderCommand::WaitOnBarrier(cmd, CreateDependencyInfo({
            .LayoutTransitionInfo = transitionInfo},
            deletionQueue));
        
        RenderCommand::BlitImage(cmd, source, destination, image.Description().MipmapFilter);
        transitionInfo = {
            .ImageSubresource = mipmapSubresource,
            .SourceStage = PipelineStage::AllCommands,
            .DestinationStage = PipelineStage::AllTransfer,
            .SourceAccess = PipelineAccess::WriteAll,
            .DestinationAccess = PipelineAccess::ReadTransfer,
            .OldLayout = ImageLayout::Destination,
            .NewLayout = ImageLayout::Source};
        RenderCommand::WaitOnBarrier(cmd, CreateDependencyInfo({
            .LayoutTransitionInfo = transitionInfo},
            deletionQueue));
    }
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

DeviceResources::ImageResource Device::CreateImageResource(ImageCreateInfo& createInfo)
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
    DeviceCheck(vmaCreateImage(Allocator(), &imageCreateInfo, &allocationInfo,
        &imageResource.Image, &imageResource.Allocation, nullptr),
        "Failed to create image");

    return imageResource;
}

void Device::Destroy(ResourceHandleType<Image> image)
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
    vmaDestroyImage(Allocator(), imageResource.Image, imageResource.Allocation);
    Resources().RemoveResource(image);
}

void Device::CreateViews(const ImageSubresource& image,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    DeviceResources::ImageResource& resource = Resources()[*image.Image];
    VkFormat viewFormat = vulkanFormatFromFormat(image.Image->m_Description.Format);
    if (additionalViews.empty())
    {
        resource.Views.ViewType.View = CreateVulkanImageView(image, viewFormat);
        resource.Views.ViewList = &resource.Views.ViewType.View;
        return;
    }

    resource.Views.ViewType.ViewCount = 1 + (u32)image.Image->m_Description.AdditionalViews.size();
    resource.Views.ViewList = new VkImageView[resource.Views.ViewType.ViewCount];
    resource.Views.ViewList[0] = CreateVulkanImageView(image, viewFormat);
    for (u32 viewIndex = 0; viewIndex < additionalViews.size(); viewIndex++)
        resource.Views.ViewList[viewIndex + 1] = CreateVulkanImageView(
            ImageSubresource{.Image = image.Image, .Description = additionalViews[viewIndex]}, viewFormat);
}

Sampler Device::CreateSampler(SamplerCreateInfo&& createInfo)
{
    const SamplerCache::CacheKey key = SamplerCache::CreateCacheKey(createInfo);
    Sampler* cached = SamplerCache::Find(key);
    if (cached)
        return *cached;
    
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
    DeviceCheck(vkCreateSampler(s_State.Device, &samplerCreateInfo, nullptr, &samplerResource.Sampler),
        "Failed to create depth pyramid sampler");

    Sampler sampler = {};
    sampler.m_ResourceHandle = Resources().AddResource(samplerResource);
    DeletionQueue().Enqueue(sampler);

    SamplerCache::Emplace(key, sampler);
    
    return sampler;
}

void Device::Destroy(ResourceHandleType<Sampler> sampler)
{
    vkDestroySampler(s_State.Device, Resources().m_Samplers[sampler.m_Id].Sampler, nullptr);
    Resources().RemoveResource(sampler);
}

RenderingAttachment Device::CreateRenderingAttachment(RenderingAttachmentCreateInfo&& createInfo)
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
    renderingAttachmentResource.AttachmentInfo.imageView = Resources()[*createInfo.Image].Views.ViewList[
        createInfo.Image->GetViewHandle(createInfo.Description.Subresource).m_Index];
    renderingAttachmentResource.AttachmentInfo.loadOp = vulkanAttachmentLoadFromAttachmentLoad(
        createInfo.Description.OnLoad);
    renderingAttachmentResource.AttachmentInfo.storeOp = vulkanAttachmentStoreFromAttachmentStore(
        createInfo.Description.OnStore);
    renderingAttachmentResource.AttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;

    RenderingAttachment renderingAttachment = {};
    renderingAttachment.m_Type = createInfo.Description.Type;
    renderingAttachment.m_ResourceHandle = Resources().AddResource(renderingAttachmentResource);
    
    return renderingAttachment;
}

void Device::Destroy(ResourceHandleType<RenderingAttachment> renderingAttachment)
{
    Resources().RemoveResource(renderingAttachment);
}

RenderingInfo Device::CreateRenderingInfo(RenderingInfoCreateInfo&& createInfo)
{
    DeviceResources::RenderingInfoResource renderingInfoResource = {};
    renderingInfoResource.ColorAttachments.reserve(createInfo.ColorAttachments.size());
    
    for (auto& attachment : createInfo.ColorAttachments)
        renderingInfoResource.ColorAttachments.push_back(Resources()[attachment].AttachmentInfo);
    if (createInfo.DepthAttachment.has_value())
        renderingInfoResource.DepthAttachment = Resources()[*createInfo.DepthAttachment].AttachmentInfo;

    RenderingInfo renderingInfo = {};
    renderingInfo.m_RenderArea = createInfo.RenderArea;
    renderingInfo.m_ResourceHandle = Resources().AddResource(renderingInfoResource);
    
    return renderingInfo;
}

void Device::Destroy(ResourceHandleType<RenderingInfo> renderingInfo)
{
    Resources().RemoveResource(renderingInfo);
}

PipelineLayout Device::CreatePipelineLayout(PipelineLayoutCreateInfo&& createInfo)
{
    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(createInfo.PushConstants.size());
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.reserve(createInfo.DescriptorSetLayouts.size());
    for (auto& pushConstant : createInfo.PushConstants)
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.size = pushConstant.SizeBytes;
        pushConstantRange.offset = pushConstant.Offset;
        pushConstantRange.stageFlags = vulkanShaderStageFromShaderStage(pushConstant.StageFlags);
    
        pushConstantRanges.push_back(pushConstantRange);
    }
    for (auto& descriptorLayout : createInfo.DescriptorSetLayouts)
        descriptorSetLayouts.push_back(Resources()[descriptorLayout].Layout);
    
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pushConstantRangeCount = (u32)pushConstantRanges.size();
    layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
    layoutCreateInfo.setLayoutCount = (u32)descriptorSetLayouts.size();
    layoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

    DeviceResources::PipelineLayoutResource pipelineLayoutResource = {};
    pipelineLayoutResource.PushConstants = pushConstantRanges;
    DeviceCheck(vkCreatePipelineLayout(s_State.Device, &layoutCreateInfo, nullptr, &pipelineLayoutResource.Layout),
        "Failed to create pipeline layout");

    PipelineLayout layout = {};
    layout.m_ResourceHandle = Resources().AddResource(pipelineLayoutResource);

    return layout;
}

void Device::Destroy(ResourceHandleType<PipelineLayout> pipelineLayout)
{
    vkDestroyPipelineLayout(s_State.Device, Resources().m_PipelineLayouts[pipelineLayout.m_Id].Layout, nullptr);
    Resources().RemoveResource(pipelineLayout);
}

Pipeline Device::CreatePipeline(PipelineCreateInfo&& createInfo)
{
    VkPipelineLayout layout = Resources()[createInfo.PipelineLayout].Layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    shaders.reserve(createInfo.Shaders.size());
    for (auto& shader : createInfo.Shaders)
    {
        auto& module = Resources()[shader];

        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.module = module.Module;
        shaderStageCreateInfo.stage = module.Stage;
        shaderStageCreateInfo.pName = "main";

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
        if (createInfo.UseDescriptorBuffer)
            pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        DeviceResources::PipelineResource pipelineResource = {};
        DeviceCheck(vkCreateComputePipelines(s_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create compute pipeline");
        pipeline.m_ResourceHandle = Resources().AddResource(pipelineResource);
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
        depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
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
        if (createInfo.UseDescriptorBuffer)
            pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        DeviceResources::PipelineResource pipelineResource = {};
        DeviceCheck(vkCreateGraphicsPipelines(s_State.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create graphics pipeline");
        pipeline.m_ResourceHandle = Resources().AddResource(pipelineResource);
    }

    return pipeline;
}

void Device::Destroy(ResourceHandleType<Pipeline> pipeline)
{
    vkDestroyPipeline(s_State.Device, Resources().m_Pipelines[pipeline.m_Id].Pipeline, nullptr);
    Resources().RemoveResource(pipeline);
}

ShaderModule Device::CreateShaderModule(ShaderModuleCreateInfo&& createInfo)
{
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = createInfo.Source.size();
    moduleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.Source.data());

    DeviceResources::ShaderModuleResource shaderModuleResource = {};
    DeviceCheck(vkCreateShaderModule(s_State.Device, &moduleCreateInfo, nullptr, &shaderModuleResource.Module),
         "Failed to create shader module");
    shaderModuleResource.Stage = vulkanStageBitFromShaderStage(createInfo.Stage);
    
    ShaderModule module = {};
    module.m_ResourceHandle = Resources().AddResource(shaderModuleResource);

    return module;
}

void Device::Destroy(ResourceHandleType<ShaderModule> shaderModule)
{
    vkDestroyShaderModule(s_State.Device, Resources().m_ShaderModules[shaderModule.m_Id].Module, nullptr);
    Resources().RemoveResource(shaderModule);
}

DescriptorsLayout Device::CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo)
{
    static SamplerBorderColor black = SamplerBorderColor::Black;
    static SamplerBorderColor white = SamplerBorderColor::White;
    static Sampler immutableSampler = getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::Repeat, black);
    static Sampler immutableSamplerNearest = getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::Repeat, black);
    static Sampler immutableSamplerClampEdge =
        getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampEdge, black);
    static Sampler immutableSamplerNearestClampEdge =
        getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampEdge, black);
    static Sampler immutableSamplerClampBlack =
        getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampBorder, black);
    static Sampler immutableSamplerNearestClampBlack =
        getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampBorder, black);
    static Sampler immutableSamplerClampWhite =
        getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampBorder, white);
    static Sampler immutableSamplerNearestClampWhite =
        getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampBorder, white);
    
    static Sampler immutableShadowSampler =
        getImmutableShadowSampler(ImageFilter::Linear, SamplerDepthCompareMode::Less); 
    static Sampler immutableShadowNearestSampler =
        getImmutableShadowSampler(ImageFilter::Nearest, SamplerDepthCompareMode::Less);

    ASSERT(createInfo.BindingFlags.size() == createInfo.Bindings.size(),
        "If any element of binding flags is set, every element has to be set")


    const DescriptorLayoutCache::CacheKey key = DescriptorLayoutCache::CreateCacheKey(createInfo);
    DescriptorsLayout* cached = DescriptorLayoutCache::Find(key);
    if (cached)
        return *cached;
    
    std::vector<VkDescriptorBindingFlags> bindingFlags;
    bindingFlags.reserve(createInfo.BindingFlags.size());
    for (auto flag : createInfo.BindingFlags)
        bindingFlags.push_back(vulkanDescriptorBindingFlagsFromDescriptorFlags(flag));
    
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(createInfo.Bindings.size());
    DescriptorLayoutFlags layoutFlags = createInfo.Flags;
    for (auto& binding : createInfo.Bindings)
    {
        bindings.push_back({
            .binding = binding.Binding,
            .descriptorType = vulkanDescriptorTypeFromDescriptorType(binding.Type),
            .descriptorCount = binding.Count,
            .stageFlags = vulkanShaderStageFromShaderStage(binding.Shaders),
            .pImmutableSamplers = nullptr});

        if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampEdge))
            bindings.back().pImmutableSamplers = &Resources()[immutableSamplerClampEdge].Sampler;

        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampEdge))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearestClampEdge].Sampler;

        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampBlack))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerClampBlack].Sampler;

        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampBlack))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearestClampBlack].Sampler;

        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampWhite))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerClampWhite].Sampler;
        
        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampWhite))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearestClampWhite].Sampler;
        
        else if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearest))
            bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearest].Sampler;
        
        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerShadow))
                bindings.back().pImmutableSamplers = &Resources()[immutableShadowSampler].Sampler;
        
        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerShadowNearest))
            bindings.back().pImmutableSamplers = &Resources()[immutableShadowNearestSampler].Sampler;

        else if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSampler))
            bindings.back().pImmutableSamplers = &Resources()[immutableSampler].Sampler;
    }
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.bindingCount = (u32)bindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();
    
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = (u32)bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.flags = vulkanDescriptorsLayoutFlagsFromDescriptorsLayoutFlags(layoutFlags);
    layoutCreateInfo.pNext = &bindingFlagsCreateInfo;

    DeviceResources::DescriptorSetLayoutResource descriptorSetLayoutResource = {};
    DeviceCheck(vkCreateDescriptorSetLayout(s_State.Device, &layoutCreateInfo, nullptr,
        &descriptorSetLayoutResource.Layout), "Failed to create descriptor set layout");
    
    DescriptorsLayout layout = {};
    layout.m_ResourceHandle = Resources().AddResource(descriptorSetLayoutResource);
    DeletionQueue().Enqueue(layout);

    DescriptorLayoutCache::Emplace(key, layout);
    
    return layout;
}

void Device::Destroy(ResourceHandleType<DescriptorsLayout> layout)
{
    vkDestroyDescriptorSetLayout(s_State.Device, Resources().m_DescriptorLayouts[layout.m_Id].Layout, nullptr);
    Resources().RemoveResource(layout);
}

DescriptorSet Device::CreateDescriptorSet(DescriptorSetCreateInfo&& createInfo)
{
    // prepare 'bindless' descriptors info
    std::vector<DescriptorSetCreateInfo::VariableBindingInfo> variableBindingInfos;
    variableBindingInfos.assign_range(createInfo.VariableBindings);
    std::ranges::sort(variableBindingInfos,
        [](u32 a, u32 b) { return a < b; },
        [](const DescriptorSetCreateInfo::VariableBindingInfo& v) { return v.Slot; });
    std::vector<u32> variableBindingCounts(variableBindingInfos.size());
    for (u32 i = 0; i < variableBindingInfos.size(); i++)
        variableBindingCounts[i] = variableBindingInfos[i].Count;
    
    // create empty descriptor set
    DescriptorSet descriptorSet = {};
    descriptorSet.m_Allocator = createInfo.Allocator;
    descriptorSet.m_Layout = createInfo.Layout;
    
    createInfo.Allocator->Allocate(descriptorSet, createInfo.PoolFlags, variableBindingCounts);
    Resources().MapDescriptorSetToAllocator(descriptorSet, *createInfo.Allocator);
    
    // convert bound resources
    std::vector<VkDescriptorBufferInfo> boundBuffers;
    boundBuffers.reserve(createInfo.Buffers.size());
    std::vector<VkDescriptorImageInfo> boundTextures;
    boundTextures.reserve(createInfo.Textures.size());
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(boundBuffers.size() + boundTextures.size());

    for (auto& buffer : createInfo.Buffers)
    {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;
        write.dstSet = Resources()[descriptorSet].DescriptorSet;
        write.descriptorType = vulkanDescriptorTypeFromDescriptorType(buffer.Type);
        u32 slot = buffer.Slot;
        write.dstBinding = slot;
        
        const DeviceResources::BufferResource& bufferResource = Resources()[*buffer.BindingInfo.Buffer];
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = bufferResource.Buffer;
        descriptorBufferInfo.offset = buffer.BindingInfo.Description.Offset;
        descriptorBufferInfo.range = buffer.BindingInfo.Description.SizeBytes;
        boundBuffers.push_back(descriptorBufferInfo);
        write.pBufferInfo = &boundBuffers.back();
        writes.push_back(write);
    }

    for (auto& texture : createInfo.Textures)
    {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;
        write.dstSet = Resources()[descriptorSet].DescriptorSet;
        write.descriptorType = vulkanDescriptorTypeFromDescriptorType(texture.Type);
        u32 slot = texture.Slot;
        write.dstBinding = slot;
        
        VkDescriptorImageInfo descriptorTextureInfo = {};
        const ImageBindingInfo& binding = texture.BindingInfo;
        descriptorTextureInfo.sampler = Resources()[binding.Sampler].Sampler;
        descriptorTextureInfo.imageView = Resources()[*binding.Image].Views.ViewList[binding.ViewHandle.m_Index];
        descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(binding.Layout);
        boundTextures.push_back(descriptorTextureInfo);
        write.pImageInfo = &boundTextures.back();
        writes.push_back(write);
    }
    
    vkUpdateDescriptorSets(s_State.Device, (u32)writes.size(), writes.data(), 0, nullptr);

    return descriptorSet;
}

void Device::AllocateDescriptorSet(DescriptorAllocator& allocator, DescriptorSet& set, DescriptorPoolFlags poolFlags,
        const std::vector<u32>& variableBindingCounts)
{
    DeviceResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
    u32 poolIndex = GetFreePoolIndexFromAllocator(allocator, poolFlags);
    VkDescriptorPool pool = allocatorResource.FreePools[poolIndex].Pool;

    VkDescriptorSetVariableDescriptorCountAllocateInfo vulkanVariableBindingCounts = {};
    vulkanVariableBindingCounts.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    vulkanVariableBindingCounts.descriptorSetCount = (u32)variableBindingCounts.size();
    vulkanVariableBindingCounts.pDescriptorCounts = variableBindingCounts.data();
    
    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &Resources()[set.m_Layout].Layout;
    allocateInfo.pNext = &vulkanVariableBindingCounts;

    DeviceResources::DescriptorSetResource descriptorSetResource = {};
    vkAllocateDescriptorSets(s_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet);
    descriptorSetResource.Pool = pool;

    if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
    {
        allocatorResource.UsedPools.push_back(allocatorResource.FreePools[poolIndex]);
        allocatorResource.FreePools.erase(allocatorResource.FreePools.begin() + poolIndex);

        poolIndex = GetFreePoolIndexFromAllocator(allocator, poolFlags);
        pool = allocatorResource.FreePools[poolIndex].Pool;
        allocateInfo.descriptorPool = pool;
        allocateInfo.pSetLayouts = &Resources()[set.m_Layout].Layout;
        DeviceCheck(vkAllocateDescriptorSets(s_State.Device, &allocateInfo, &descriptorSetResource.DescriptorSet),
            "Failed to allocate descriptor set");
        descriptorSetResource.Pool = pool;
    }
    allocatorResource.FreePools[poolIndex].AllocationCount++;

    set.m_ResourceHandle = Resources().AddResource(descriptorSetResource);
}

void Device::DeallocateDescriptorSet(ResourceHandleType<DescriptorAllocator> allocator, ResourceHandleType<DescriptorSet> set)
{
    DeviceResources::DescriptorAllocatorResource& allocatorResource =
        Resources().m_DescriptorAllocators[allocator.m_Id];
    VkDescriptorPool pool = Resources().m_DescriptorSets[set.m_Id].Pool;

    auto it = std::ranges::find(allocatorResource.FreePools, pool,
        [](const DeviceResources::DescriptorAllocatorResource::PoolInfo& info)
        {
            return info.Pool;
        });
    if (it == allocatorResource.FreePools.end())
    {
        it = std::ranges::find(allocatorResource.UsedPools, pool,
            [](const DeviceResources::DescriptorAllocatorResource::PoolInfo& info)
            {
                return info.Pool;
            });
        ASSERT(it != allocatorResource.UsedPools.end(), "Descriptor set wasn't allocated with this allocator")
        it->AllocationCount--;
        allocatorResource.FreePools.push_back(*it);
        allocatorResource.UsedPools.erase(it);
    }
    
    vkFreeDescriptorSets(s_State.Device, pool, 1, &Resources().m_DescriptorSets[set.m_Id].DescriptorSet);
    Resources().RemoveResource(set);
}

void Device::UpdateDescriptorSet(DescriptorSet& descriptorSet,
    u32 slot, const Texture& texture, DescriptorType type, u32 arrayIndex)
{
    ImageBindingInfo bindingInfo = texture.BindingInfo({}, ImageLayout::Readonly);
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = Resources()[bindingInfo.Sampler].Sampler;
    descriptorTextureInfo.imageView = Resources()[*bindingInfo.Image].Views.ViewList[bindingInfo.ViewHandle.m_Index];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(bindingInfo.Layout);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = Resources()[descriptorSet].DescriptorSet;
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = arrayIndex;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);

    vkUpdateDescriptorSets(s_State.Device, 1, &write, 0, nullptr);
}

DescriptorAllocator Device::CreateDescriptorAllocator(DescriptorAllocatorCreateInfo&& createInfo)
{
    DeviceResources::DescriptorAllocatorResource descriptorAllocatorResource = {};
    
    DescriptorAllocator allocator = {};
    allocator.m_MaxSetsPerPool = createInfo.MaxSets;
    allocator.m_ResourceHandle = Resources().AddResource(descriptorAllocatorResource);
    if (allocator.Handle().m_Id >= Resources().m_DescriptorAllocatorToSetsMap.size())
        Resources().m_DescriptorAllocatorToSetsMap.resize(allocator.Handle().m_Id + 1);

    return allocator;
}

void Device::Destroy(ResourceHandleType<DescriptorAllocator> allocator)
{
    DeviceResources::DescriptorAllocatorResource& allocatorResource =
        Resources().m_DescriptorAllocators[allocator.m_Id];
    for (auto& pool : allocatorResource.FreePools)
        vkDestroyDescriptorPool(s_State.Device, pool.Pool, nullptr);
    for (auto& pool : allocatorResource.UsedPools)
        vkDestroyDescriptorPool(s_State.Device, pool.Pool, nullptr);

    Resources().DestroyDescriptorSetsOfAllocator(allocator);
    Resources().RemoveResource(allocator);
}

void Device::ResetAllocator(DescriptorAllocator& allocator)
{
    DeviceResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkResetDescriptorPool(s_State.Device, pool.Pool, 0);
    for (auto pool : allocatorResource.UsedPools)
    {
        vkResetDescriptorPool(s_State.Device, pool.Pool, 0);
        allocatorResource.FreePools.push_back(pool);
    }
    allocatorResource.UsedPools.clear();
    Resources().DestroyDescriptorSetsOfAllocator(allocator.Handle());
}

DescriptorArenaAllocator Device::CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo)
{
    ASSERT(!createInfo.UsedTypes.empty(), "At least one descriptor type is necessary")
    
    if (createInfo.Kind == DescriptorAllocatorKind::Resources)
        for (auto type : createInfo.UsedTypes)
            ASSERT(type != DescriptorType::Sampler,
                "Cannot use allocator of this kind for requested descriptor kinds")
    else
        for (auto type : createInfo.UsedTypes)
            ASSERT(type == DescriptorType::Sampler,
                "Cannot use allocator of this kind for requested descriptor kinds")
    
    u32 maxDescriptorSize = 0;
    for (auto type : createInfo.UsedTypes)
        maxDescriptorSize = std::max(maxDescriptorSize, GetDescriptorSizeBytes(type));
    
    u64 arenaSizeBytes = (u64)maxDescriptorSize * createInfo.DescriptorCount;

    VkBufferUsageFlags usageFlags = createInfo.Kind == DescriptorAllocatorKind::Resources ?
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT : VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateFlags allocationFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    BufferUsage bufferUsage = BufferUsage::None;
    if (createInfo.Residence == DescriptorAllocatorResidence::GPU)
    {
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferUsage |= BufferUsage::Destination;
    }
    else
    {
        allocationFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    std::array<Buffer, BUFFERED_FRAMES> arenas = {};
    for (auto& arena : arenas)
    {
        DeviceResources::BufferResource arenaResource = CreateBufferResource(arenaSizeBytes,
            usageFlags, allocationFlags);
        arena.m_Description = {
            .SizeBytes = arenaSizeBytes,
            .Usage = bufferUsage};
        arena.m_HostAddress = arenaResource.Allocation->GetMappedData();
        arena.m_ResourceHandle = Resources().AddResource(arenaResource);
    }

    DescriptorArenaAllocator descriptorArenaAllocator = {};
    descriptorArenaAllocator.m_Kind = createInfo.Kind;
    descriptorArenaAllocator.m_Residence = createInfo.Residence;
    descriptorArenaAllocator.m_Buffers = arenas;

    // todo: this is actually the place for it, but device queue should come as separate parameter
    for (auto& buffer : arenas)
        DeletionQueue().Enqueue(buffer);

    return descriptorArenaAllocator;
}

std::optional<Descriptors> Device::Allocate(DescriptorArenaAllocator& allocator,
    DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings)
{
    auto& descriptorBufferProps = s_State.GPUDescriptorBufferProperties;

    // if we have bindless binding, we have to calculate layout size as a sum of bindings sizes
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
            bool isBindless = enumHasAny(binding.DescriptorFlags, assetLib::ShaderStageInfo::DescriptorSet::Bindless);
            ASSERT(
                (bindingIndex == (u32)bindings.Bindings.size() - 1 && isBindless) ||
                (bindingIndex != (u32)bindings.Bindings.size() - 1 && !isBindless),
                "Only one binding can be declared as 'bindless' for any particular set, and it has to be the last one")

            layoutSizeBytes += isBindless ? bindings.BindlessCount * GetDescriptorSizeBytes(binding.Type) :
                GetDescriptorSizeBytes(binding.Type);
        }
    }
    
    layoutSizeBytes = CoreUtils::align(layoutSizeBytes, descriptorBufferProps.descriptorBufferOffsetAlignment);
    if (layoutSizeBytes + allocator.m_CurrentOffset > allocator.m_Buffers[0].GetSizeBytes())
        return {};

    std::vector<u64> bindingOffsets(bindings.Bindings.size());
    for (u32 offsetIndex = 0; offsetIndex < bindingOffsets.size(); offsetIndex++)
    {
        auto& binding = bindings.Bindings[offsetIndex];
        vkGetDescriptorSetLayoutBindingOffsetEXT(s_State.Device, Resources()[layout].Layout, binding.Binding,
            &bindingOffsets[offsetIndex]);
        bindingOffsets[offsetIndex] += allocator.m_CurrentOffset;
    }
    
    Descriptors descriptors = {};
    descriptors.m_Offsets = bindingOffsets;
    descriptors.m_SizeBytes = layoutSizeBytes;
    descriptors.m_Allocator = &allocator;

    allocator.m_CurrentOffset += layoutSizeBytes;
    
    return descriptors;
}

void Device::UpdateDescriptors(const Descriptors& descriptors, u32 slot, const BufferBindingInfo& buffer,
    DescriptorType type, u32 index)
{
    ASSERT(type != DescriptorType::TexelStorage && type != DescriptorType::TexelUniform,
        "Texel buffers require format information")
    ASSERT(type != DescriptorType::StorageBufferDynamic && type != DescriptorType::UniformBufferDynamic,
        "Dynamic buffers are not supported when using descriptor buffer")
    
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = Resources()[*buffer.Buffer].Buffer;
    u64 deviceAddress = vkGetBufferDeviceAddress(s_State.Device, &deviceAddressInfo);

    VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
    descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    descriptorAddressInfo.address = deviceAddress + buffer.Description.Offset;
    descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
    descriptorAddressInfo.range = buffer.Description.SizeBytes;

    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    // using the fact that 'descriptorGetInfo.data' is union
    descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

    u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    u64 innerOffsetBytes = descriptorSizeBytes * index;
    ASSERT(innerOffsetBytes + descriptorSizeBytes <= descriptors.m_SizeBytes,
        "Trying to write descriptor outside of the allocated region")

    u64 offsetBytes = descriptors.m_Offsets[slot] + innerOffsetBytes;
    vkGetDescriptorEXT(s_State.Device, &descriptorGetInfo, descriptorSizeBytes,
        (u8*)descriptors.m_Allocator->GetCurrentBuffer().m_HostAddress + offsetBytes);
}

void Device::UpdateDescriptors(const Descriptors& descriptors, u32 slot, const TextureBindingInfo& texture,
    DescriptorType type, u32 index)
{
    VkDescriptorGetInfoEXT descriptorGetInfo = {};
    descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorGetInfo.type = vulkanDescriptorTypeFromDescriptorType(type);
    VkDescriptorImageInfo descriptorImageInfo;
    if (type == DescriptorType::Sampler)
    {
        descriptorGetInfo.data.pSampler = &Resources()[texture.Sampler].Sampler;
    }
    else
    {
        descriptorImageInfo.imageView = Resources()[*texture.Image].Views.ViewList[texture.ViewHandle.m_Index];
        descriptorImageInfo.imageLayout = vulkanImageLayoutFromImageLayout(texture.Layout);
        descriptorGetInfo.data.pSampledImage = &descriptorImageInfo;
    }

    u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    u64 innerOffsetBytes = descriptorSizeBytes * index;
    ASSERT(innerOffsetBytes + descriptorSizeBytes <= descriptors.m_SizeBytes,
        "Trying to write descriptor outside of the allocated region")
    
    u64 offsetBytes = descriptors.m_Offsets[slot] + innerOffsetBytes;
    vkGetDescriptorEXT(s_State.Device, &descriptorGetInfo, descriptorSizeBytes,
        (u8*)descriptors.m_Allocator->GetCurrentBuffer().m_HostAddress + offsetBytes);
}

void Device::UpdateGlobalDescriptors(const Descriptors& descriptors, u32 slot, const BufferBindingInfo& buffer,
    DescriptorType type, u32 index)
{
    u32 currentIndex = descriptors.m_Allocator->m_CurrentBuffer; 
    for (u32 i = 0; i < descriptors.m_Allocator->m_Buffers.size(); i++)
    {
        // there is no spoon
        const_cast<DescriptorArenaAllocator*>(descriptors.m_Allocator)->m_CurrentBuffer = i;
        UpdateDescriptors(descriptors, slot, buffer, type, index);
    }
    const_cast<DescriptorArenaAllocator*>(descriptors.m_Allocator)->m_CurrentBuffer = currentIndex;
}

void Device::UpdateGlobalDescriptors(const Descriptors& descriptors, u32 slot, const TextureBindingInfo& texture,
    DescriptorType type, u32 index)
{
    u32 currentIndex = descriptors.m_Allocator->m_CurrentBuffer; 
    for (u32 i = 0; i < descriptors.m_Allocator->m_Buffers.size(); i++)
    {
        // there is no spoon
        const_cast<DescriptorArenaAllocator*>(descriptors.m_Allocator)->m_CurrentBuffer = i;
        UpdateDescriptors(descriptors, slot, texture, type, index);
    }
    const_cast<DescriptorArenaAllocator*>(descriptors.m_Allocator)->m_CurrentBuffer = currentIndex;
}

u32 Device::GetDescriptorSizeBytes(DescriptorType type)
{
    auto& props = s_State.GPUDescriptorBufferProperties;
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

Fence Device::CreateFence(FenceCreateInfo&& createInfo, ::DeletionQueue& deletionQueue)
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (createInfo.IsSignaled)
        fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        fenceCreateInfo.flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    DeviceResources::FenceResource fenceResource = {};    
    DeviceCheck(vkCreateFence(s_State.Device, &fenceCreateInfo, nullptr, &fenceResource.Fence),
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

void Device::WaitForFence(const Fence& fence)
{
    DeviceCheck(vkWaitForFences(s_State.Device, 1, &Resources()[fence].Fence, true, 10'000'000'000),
        "Error while waiting for fences");
}

bool Device::CheckFence(const Fence& fence)
{
    const VkResult result = vkGetFenceStatus(s_State.Device, Resources()[fence].Fence);
    return result == VK_SUCCESS;
}

void Device::ResetFence(const Fence& fence)
{
    DeviceCheck(vkResetFences(s_State.Device, 1, &Resources()[fence].Fence), "Error while resetting fences");
}

Semaphore Device::CreateSemaphore(::DeletionQueue& deletionQueue)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    DeviceResources::SemaphoreResource semaphoreResource = {};
    DeviceCheck(vkCreateSemaphore(s_State.Device, &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore),
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

void Device::TimelineSemaphoreWaitCPU(const TimelineSemaphore& semaphore, u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &Resources()[semaphore].Semaphore;
    waitInfo.pValues = &value;
    
    DeviceCheck(vkWaitSemaphores(s_State.Device, &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void Device::TimelineSemaphoreSignalCPU(TimelineSemaphore& semaphore, u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = Resources()[semaphore].Semaphore;
    signalInfo.value = value;

    DeviceCheck(vkSignalSemaphore(s_State.Device, &signalInfo),
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

        dependencyInfoResource.ExecutionMemoryDependenciesInfo.push_back(memoryBarrier);
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

        dependencyInfoResource.ExecutionMemoryDependenciesInfo.push_back(memoryBarrier);
    }
    if (createInfo.LayoutTransitionInfo.has_value())
    {
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
        imageMemoryBarrier.image = Resources()[*createInfo.LayoutTransitionInfo->ImageSubresource.Image].Image;
        imageMemoryBarrier.subresourceRange = {
            .aspectMask = vulkanImageAspectFromImageUsage(
                createInfo.LayoutTransitionInfo->ImageSubresource.Image->m_Description.Usage),
            .baseMipLevel = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.MipmapBase,
            .levelCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Mipmaps,
            .baseArrayLayer = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.LayerBase,
            .layerCount = (u32)createInfo.LayoutTransitionInfo->ImageSubresource.Description.Layers};

        dependencyInfoResource.LayoutTransitionsInfo.push_back(imageMemoryBarrier);
    }

    DependencyInfo dependencyInfo = Resources().AddResource(dependencyInfoResource);
    deletionQueue.Enqueue(dependencyInfo);
    
    return dependencyInfo;
}

void Device::Destroy(DependencyInfo dependencyInfo)
{
    Resources().RemoveResource(dependencyInfo);
}

SplitBarrier Device::CreateSplitBarrier()
{
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    DeviceResources::SplitBarrierResource splitBarrierResource = {};
    DeviceCheck(vkCreateEvent(s_State.Device, &eventCreateInfo, nullptr, &splitBarrierResource.Event),
        "Failed to create split barrier");

    SplitBarrier splitBarrier = {};
    splitBarrier.m_ResourceHandle = Resources().AddResource(splitBarrierResource);
    
    return splitBarrier;
}

void Device::Destroy(ResourceHandleType<SplitBarrier> splitBarrier)
{
    vkDestroyEvent(s_State.Device, Resources().m_SplitBarriers[splitBarrier.m_Id].Event, nullptr);
    Resources().RemoveResource(splitBarrier);
}

u32 Device::GetFreePoolIndexFromAllocator(DescriptorAllocator& allocator, DescriptorPoolFlags poolFlags)
{
    DeviceResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
    for (u32 i = 0; i < allocatorResource.FreePools.size(); i++)
        if (allocatorResource.FreePools[i].Flags == poolFlags)
            return i;

    // the pool does not exist yet
    u32 index = (u32)allocatorResource.FreePools.size();
    std::vector<VkDescriptorPoolSize> sizes(allocator.m_PoolSizes.size());
    for (u32 i = 0; i < sizes.size(); i++)
        sizes[i] = {
            .type = vulkanDescriptorTypeFromDescriptorType(allocator.m_PoolSizes[i].DescriptorType),
            .descriptorCount = (u32)(allocator.m_PoolSizes[i].SetSizeMultiplier * (f32)allocator.m_MaxSetsPerPool)};

    VkDescriptorPool pool = {};
    
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = allocator.m_MaxSetsPerPool;
    poolCreateInfo.poolSizeCount = (u32)sizes.size();
    poolCreateInfo.pPoolSizes = sizes.data();
    poolCreateInfo.flags = vulkanDescriptorPoolFlagsFromDescriptorPoolFlags(poolFlags);

    DeviceCheck(vkCreateDescriptorPool(s_State.Device, &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    allocatorResource.FreePools.push_back({.Pool = pool, .Flags = poolFlags});

    return index;
}

void Device::CreateInstance(const DeviceCreateInfo& createInfo)
{
    auto checkInstanceExtensions = [](const DeviceCreateInfo& createInfo)
    {
        u32 availableExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

        return Utils::checkArrayContainsSubArray(createInfo.InstanceExtensions, availableExtensions,
            [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
            [](const char* req) { LOG("Unsupported instance extension: {}\n", req); });
    };
    auto checkInstanceValidationLayers = [](const DeviceCreateInfo& createInfo)
    {
        u32 availableValidationLayerCount = 0;
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(availableValidationLayerCount);
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, availableLayers.data());

        return Utils::checkArrayContainsSubArray(createInfo.InstanceValidationLayers, availableLayers,
            [](const char* req, const VkLayerProperties& avail) { return std::strcmp(req, avail.layerName); },
            [](const char* req) { LOG("Unsupported validation layer: {}\n", req); });
    };
    
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
    DeviceCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &s_State.Instance),
        "Failed to create instance\n");

    volkLoadInstance(s_State.Instance);
}

void Device::CreateSurface(const DeviceCreateInfo& createInfo)
{
    ASSERT(createInfo.Window != nullptr, "Window pointer is unset")
    s_State.Window = createInfo.Window;
    DeviceCheck(glfwCreateWindowSurface(s_State.Instance, createInfo.Window, nullptr, &s_State.Surface),
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
    
        DeviceQueues queues = {};
    
        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                queues.Graphics.Family == QueueInfo::UNSET_FAMILY)
                queues.Graphics.Family = i;

            if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && queues.Compute.Family == QueueInfo::UNSET_FAMILY)
                if (!dedicatedCompute || !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    queues.Compute.Family = i;
        
            VkBool32 isPresentationSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, s_State.Surface, &isPresentationSupported);
            if (isPresentationSupported && queues.Presentation.Family == QueueInfo::UNSET_FAMILY)
                queues.Presentation.Family = i;

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

            return Utils::checkArrayContainsSubArray(createInfo.DeviceExtensions, availableExtensions,
                [](const char* req, const VkExtensionProperties& avail)
                {
                    return std::strcmp(req, avail.extensionName);
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
            
            return
                    features.features.samplerAnisotropy == VK_TRUE &&
                    features.features.multiDrawIndirect == VK_TRUE &&
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
                    deviceVulkan12Features.shaderBufferInt64Atomics == VK_TRUE &&
                    deviceVulkan12Features.timelineSemaphore == VK_TRUE &&
                    deviceVulkan12Features.bufferDeviceAddress == VK_TRUE &&
                    deviceVulkan12Features.scalarBlockLayout == VK_TRUE &&
                    deviceVulkan13Features.dynamicRendering == VK_TRUE &&
                    deviceVulkan13Features.synchronization2 == VK_TRUE &&
                    conditionalRenderingFeaturesExt.conditionalRendering == VK_TRUE &&
                    physicalDeviceIndexTypeUint8FeaturesExt.indexTypeUint8 == VK_TRUE &&
                    physicalDeviceDescriptorBufferFeaturesExt.descriptorBuffer == VK_TRUE;
        };
        
        
        DeviceQueues deviceQueues = findQueueFamilies(gpu, createInfo.AsyncCompute);
        if (!deviceQueues.IsComplete())
            return false;

        bool isEveryExtensionSupported = checkGPUExtensions(gpu, createInfo);
        if (!isEveryExtensionSupported)
            return false;
    
        SurfaceDetails surfaceDetails = getSurfaceDetails(gpu, s_State.Surface);
        if (!surfaceDetails.IsSufficient())
            return false;

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
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueFamilies.size());
    f32 queuePriority = 1.0f;
    for (u32 i = 0; i < queueFamilies.size(); i++)
    {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = queueFamilies[i];
        queueCreateInfos[i].queueCount = 1;
        queueCreateInfos[i].pQueuePriorities = &queuePriority; 
    }

    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
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
    physicalDeviceDescriptorBufferFeaturesExt.descriptorBuffer = VK_TRUE;
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &physicalDeviceDescriptorBufferFeaturesExt;
    deviceCreateInfo.queueCreateInfoCount = (u32)queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = (u32)createInfo.DeviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = createInfo.DeviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    DeviceCheck(vkCreateDevice(s_State.GPU, &deviceCreateInfo, nullptr, &s_State.Device),
        "Failed to create device\n");

    volkLoadDevice(s_State.Device);
}

void Device::RetrieveDeviceQueues()
{
    s_State.Queues.Graphics.m_ResourceHandle = Resources().AddResource(DeviceResources::QueueResource{});
    s_State.Queues.Presentation.m_ResourceHandle = Resources().AddResource(DeviceResources::QueueResource{});
    s_State.Queues.Compute.m_ResourceHandle = Resources().AddResource(DeviceResources::QueueResource{});

    DeletionQueue().Enqueue(s_State.Queues.Graphics);
    DeletionQueue().Enqueue(s_State.Queues.Presentation);
    DeletionQueue().Enqueue(s_State.Queues.Compute);
    
    vkGetDeviceQueue(s_State.Device, s_State.Queues.Graphics.Family, 0,
        &Resources()[s_State.Queues.Graphics].Queue);
    vkGetDeviceQueue(s_State.Device, s_State.Queues.Presentation.Family, 0,
        &Resources()[s_State.Queues.Presentation].Queue);
    vkGetDeviceQueue(s_State.Device, s_State.Queues.Compute.Family, 0,
        &Resources()[s_State.Queues.Compute].Queue);
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
    DeviceCheck(volkInitialize(), "Failed to initialize volk");

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

    s_State.SubmitContext.CommandPool = CreateCommandPool({
        .QueueKind = QueueKind::Graphics});
    DeletionQueue().Enqueue(s_State.SubmitContext.CommandPool);
    s_State.SubmitContext.CommandBuffer = s_State.SubmitContext.CommandPool.AllocateBuffer(CommandBufferKind::Primary);
    s_State.SubmitContext.Fence = CreateFence({});
    s_State.SubmitContext.QueueKind = QueueKind::Graphics;

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

ImmediateSubmitContext* Device::SubmitContext()
{
    return &s_State.SubmitContext;
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
    std::array poolSizes = {
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
    imguiInitInfo.Queue = Resources()[s_State.Queues.Graphics].Queue;
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
        return vkGetInstanceProcAddr(*(VkInstance*)instance, functionName);
    }, &s_State.Instance);
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

TracyVkCtx Device::CreateTracyGraphicsContext(const CommandBuffer& cmd)
{
    TracyVkCtx context = TracyVkContext(s_State.GPU, s_State.Device,
        Resources()[s_State.Queues.Graphics].Queue, Resources()[cmd].CommandBuffer)
    return context;
}

void Device::DestroyTracyGraphicsContext(TracyVkCtx context)
{
    TracyVkDestroy(context)
}

VkCommandBuffer Device::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return Resources()[*context->m_GraphicsCommandBuffers[context->m_CurrentFrame]].CommandBuffer;
}

ImTextureID Device::CreateImGuiImage(const ImageSubresource& texture, Sampler sampler, ImageLayout layout)
{
    ImageViewHandle viewHandle = texture.Image->GetViewHandle(texture.Description);
    VkDescriptorSet imageDescriptorSet = ImGui_ImplVulkan_AddTexture(Resources()[sampler].Sampler,
        Resources()[*texture.Image].Views.ViewList[viewHandle.m_Index],
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

VmaAllocator& Device::Allocator()
{
    return s_State.Allocator;
}

VkImageView Device::CreateVulkanImageView(const ImageSubresource& image, VkFormat format)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = Resources()[*image.Image].Image;
    createInfo.format = format;
    createInfo.viewType = vulkanImageViewTypeFromImageAndViewKind(image.Image->m_Description.Kind,
        image.Description.ImageViewKind);
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = vulkanImageAspectFromImageUsage(
        image.Image->m_Description.Usage);
    createInfo.subresourceRange.baseMipLevel = (u32)(i32)image.Description.MipmapBase;
    createInfo.subresourceRange.levelCount = (u32)(i32)image.Description.Mipmaps;
    createInfo.subresourceRange.baseArrayLayer = (u32)(i32)image.Description.LayerBase;
    createInfo.subresourceRange.layerCount = (u32)(i32)image.Description.Layers;

    VkImageView imageView;

    DeviceCheck(vkCreateImageView(s_State.Device, &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

std::pair<VkBlitImageInfo2, VkImageBlit2> Device::CreateVulkanBlitInfo(const ImageBlitInfo& source,
    const ImageBlitInfo& destination, ImageFilter filter)
{
    VkImageBlit2 imageBlit = {};
    VkBlitImageInfo2 blitImageInfo = {};
    
    imageBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    imageBlit.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(source.Image->m_Description.Usage);
    imageBlit.srcSubresource.baseArrayLayer = source.LayerBase;
    imageBlit.srcSubresource.layerCount = source.Layers;
    imageBlit.srcSubresource.mipLevel = source.MipmapBase;
    imageBlit.srcOffsets[0] = VkOffset3D{
        .x = (i32)source.Bottom.x,
        .y = (i32)source.Bottom.y,
        .z = (i32)source.Bottom.z};
    imageBlit.srcOffsets[1] = VkOffset3D{
        .x = (i32)source.Top.x,
        .y = (i32)source.Top.y,
        .z = (i32)source.Top.z};

    imageBlit.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destination.Image->m_Description.Usage);
    imageBlit.dstSubresource.baseArrayLayer = destination.LayerBase;
    imageBlit.dstSubresource.layerCount = destination.Layers;
    imageBlit.dstSubresource.mipLevel = destination.MipmapBase;
    imageBlit.dstOffsets[0] = VkOffset3D{
        .x = (i32)destination.Bottom.x,
        .y = (i32)destination.Bottom.y,
        .z = (i32)destination.Bottom.z};
    imageBlit.dstOffsets[1] = VkOffset3D{
        .x = (i32)destination.Top.x,
        .y = (i32)destination.Top.y,
        .z = (i32)destination.Top.z};
    
    blitImageInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitImageInfo.srcImage = Resources()[*source.Image].Image;
    blitImageInfo.dstImage = Resources()[*destination.Image].Image;
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.filter = vulkanFilterFromImageFilter(filter);

    return {blitImageInfo, imageBlit};
}

std::pair<VkCopyImageInfo2, VkImageCopy2> Device::CreateVulkanImageCopyInfo(const ImageCopyInfo& source,
    const ImageCopyInfo& destination)
{
    glm::uvec3 extentSource = source.Top - source.Bottom;
    glm::uvec3 extentDestination = destination.Top - destination.Bottom;
    ASSERT(extentSource == extentDestination, "Extents of source and destination must match for image copy")

    VkImageCopy2 imageCopy = {};
    VkCopyImageInfo2 copyImageInfo = {};
    
    imageCopy.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
    imageCopy.extent = VkExtent3D{
        .width = extentSource.x,
        .height = extentSource.y,
        .depth = extentSource.z};
    imageCopy.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(source.Image->m_Description.Usage);
    imageCopy.srcSubresource.baseArrayLayer = source.LayerBase;
    imageCopy.srcSubresource.layerCount = source.Layers;
    imageCopy.srcSubresource.mipLevel = source.MipmapBase;
    imageCopy.srcOffset = VkOffset3D{
        .x = (i32)source.Bottom.x,
        .y = (i32)source.Bottom.y,
        .z = (i32)source.Bottom.z};
    imageCopy.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destination.Image->m_Description.Usage);
    imageCopy.dstSubresource.baseArrayLayer = destination.LayerBase;
    imageCopy.dstSubresource.layerCount = destination.Layers;
    imageCopy.dstSubresource.mipLevel = destination.MipmapBase;
    imageCopy.dstOffset = VkOffset3D{
        .x = (i32)destination.Bottom.x,
        .y = (i32)destination.Bottom.y,
        .z = (i32)destination.Bottom.z};
    
    copyImageInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
    copyImageInfo.srcImage = Resources()[*source.Image].Image;
    copyImageInfo.dstImage = Resources()[*destination.Image].Image;
    copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImageInfo.regionCount = 1;

    return {copyImageInfo, imageCopy};
}

VkBufferImageCopy2 Device::CreateVulkanImageCopyInfo(const ImageSubresource& subresource)
{
    ASSERT(subresource.Description.Mipmaps == 1, "Buffer to image copies one mipmap at a time")
    
    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.imageExtent = {
        .width = subresource.Image->m_Description.Width,
        .height = subresource.Image->m_Description.Height,
        .depth =  subresource.Image->m_Description.GetDepth()};
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(
        subresource.Image->m_Description.Usage);
    bufferImageCopy.imageSubresource.mipLevel = (u32)(i32)subresource.Description.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = (u32)(i32)subresource.Description.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = (u32)(i32)subresource.Description.Layers;

    return bufferImageCopy;
}

std::vector<VkSemaphoreSubmitInfo> Device::CreateVulkanSemaphoreSubmit(const std::vector<Semaphore*>& semaphores,
    const std::vector<PipelineStage>& waitStages)
{
    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.reserve(semaphores.size());
    for (u32 i = 0; i < semaphores.size(); i++)
        waitSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = Resources()[*semaphores[i]].Semaphore,
            .stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i])});

    return waitSemaphoreSubmitInfos;
}

std::vector<VkSemaphoreSubmitInfo> Device::CreateVulkanSemaphoreSubmit(
    const std::vector<TimelineSemaphore*>& semaphores, const std::vector<u64>& waitValues,
    const std::vector<PipelineStage>& waitStages)
{
    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.reserve(semaphores.size());
    for (u32 i = 0; i < semaphores.size(); i++)
        waitSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = Resources()[*semaphores[i]].Semaphore,
            .value = waitValues[i],
            .stageMask = vulkanPipelineStageFromPipelineStage(waitStages[i])});

    return waitSemaphoreSubmitInfos;
}
