#include "Driver.h"

#include "Core/core.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

#include "ResourceUploader.h"
#include "Core/CoreUtils.h"
#include "Rendering/Buffer.h"
#include "Core/ProfilerContext.h"
#include "Rendering/FormatTraits.h"
#include "utils/utils.h"

#include "GLFW/glfw3.h"

namespace
{
    static_assert(ImageSubresourceDescription::ALL_MIPMAPS == VK_REMAINING_MIP_LEVELS, "Incorrect value for `ALL_MIPMAPS`");
    static_assert(ImageSubresourceDescription::ALL_LAYERS == VK_REMAINING_ARRAY_LAYERS, "Incorrect value for `ALL_LAYERS`");
    static_assert(Sampler::LOD_MAX == VK_LOD_CLAMP_NONE, "Incorrect value for `LOD_MAX`");
    
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

    VkBufferUsageFlags vulkanBufferUsageFromUsage(BufferUsage kind)
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
        default:
            ASSERT(false, "Unsupported image kind")
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

    VkCommandBufferLevel vulkanBufferLevelFromBufferKind(CommandBufferKind kind)
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

    constexpr VkPipelineStageFlags2 vulkanPipelineStageFromPipelineStage(PipelineStage stage)
    {
        std::vector<std::pair<PipelineStage, VkPipelineStageFlags2>> MAPPINGS {
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
        std::vector<std::pair<PipelineAccess, VkAccessFlagBits2>> MAPPINGS {
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
        std::vector<std::pair<PipelineDependencyFlags, VkDependencyFlags>> MAPPINGS {
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

    VkShaderStageFlagBits vulkanStageBitFromShaderStage(ShaderStage stage)
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

    VkPrimitiveTopology vulkanTopologyFromPrimitiveKind(PrimitiveKind kind)
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
}

void DeletionQueue::Flush()
{
    for (auto handle : m_Buffers)
        Driver::Destroy(handle);
    for (auto handle : m_Images)
        Driver::Destroy(handle);
    for (auto handle : m_Samplers)
        Driver::Destroy(handle);
    
    for (auto handle : m_CommandPools)
        Driver::Destroy(handle);
    
    for (auto handle : m_DescriptorLayouts)
        Driver::Destroy(handle);
    for (auto handle : m_DescriptorAllocators)
        Driver::Destroy(handle);
    
    for (auto handle : m_PipelineLayouts)
        Driver::Destroy(handle);
    for (auto handle : m_Pipelines)
        Driver::Destroy(handle);
    
    for (auto handle : m_RenderingAttachments)
        Driver::Destroy(handle);
    for (auto handle : m_RenderingInfos)
        Driver::Destroy(handle);
    
    for (auto handle : m_Fences)
        Driver::Destroy(handle);
    for (auto handle : m_Semaphores)
        Driver::Destroy(handle);
    for (auto handle : m_DependencyInfos)
        Driver::Destroy(handle);
    for (auto handle : m_SplitBarriers)
        Driver::Destroy(handle);
    
    for (auto handle : m_Shaders)
        Driver::Destroy(handle);
    
    for (auto handle : m_Queues)
        Driver::Destroy(handle);
    
    for (auto handle : m_Swapchains)
        Driver::Destroy(handle);
    
    m_Swapchains.clear();
    m_Buffers.clear();
    m_Images.clear();
    m_Samplers.clear();
    m_ViewLists.clear();
    m_CommandPools.clear();
    m_Queues.clear();
    m_DescriptorLayouts.clear();
    m_DescriptorAllocators.clear();
    m_PipelineLayouts.clear();
    m_Pipelines.clear();
    m_RenderingAttachments.clear();
    m_RenderingInfos.clear();
    m_Fences.clear();
    m_Semaphores.clear();
    m_DependencyInfos.clear();
    m_SplitBarriers.clear();
    m_Shaders.clear();
}

void DriverResources::MapCmdToPool(const CommandBuffer& cmd, const CommandPool& pool)
{
    m_CommandPoolToBuffersMap[pool.Handle().m_Index].push_back(cmd.Handle().m_Index);
}

void DriverResources::DestroyCmdsOfPool(ResourceHandle<CommandPool> pool)
{
    for (auto index : m_CommandPoolToBuffersMap[pool.m_Index])
        m_CommandBuffers.Remove(index);
    m_DeallocatedCount += (u32)m_CommandPoolToBuffersMap[pool.m_Index].size(); 
    m_CommandPoolToBuffersMap[pool.m_Index].clear();
}


void DriverResources::MapDescriptorSetToAllocator(const DescriptorSet& set, const DescriptorAllocator& allocator)
{
    m_DescriptorAllocatorToSetsMap[allocator.Handle().m_Index].push_back(set.Handle().m_Index);
}

void DriverResources::DestroyDescriptorSetsOfAllocator(ResourceHandle<DescriptorAllocator> allocator)
{
    for (auto index : m_DescriptorAllocatorToSetsMap[allocator.m_Index])
        m_DescriptorSets.Remove(index);
    m_DeallocatedCount += (u32)m_DescriptorAllocatorToSetsMap[allocator.m_Index].size(); 
    m_DescriptorAllocatorToSetsMap[allocator.m_Index].clear();
}

DriverState Driver::s_State = DriverState{};

Device Driver::Create(const Device::Builder::CreateInfo& createInfo)
{
    DriverCheck(volkInitialize(), "Failed to initialize volk");

    DriverResources::DeviceResource deviceResource = {};
    
    Device device = {};
    CreateInstance(createInfo, deviceResource);
    CreateSurface(createInfo, deviceResource, device);
    ChooseGPU(createInfo, deviceResource, device);
    CreateDevice(createInfo, deviceResource, device);
    RetrieveDeviceQueues(deviceResource, device);

#ifdef VULKAN_VAL_LAYERS
    CreateDebugUtilsMessenger(deviceResource);
#endif

    device.m_ResourceHandle = Resources().AddResource(deviceResource);
    return device;
}

void Driver::Destroy(ResourceHandle<Device> device)
{
    DriverResources::DeviceResource& deviceResource = Resources().m_Devices[device.m_Index];
#ifdef VULKAN_VAL_LAYERS
    DestroyDebugUtilsMessenger(deviceResource);
#endif
    
    vkDestroyDevice(deviceResource.Device, nullptr);
    vkDestroySurfaceKHR(deviceResource.Instance, deviceResource.Surface, nullptr);
    vkDestroyInstance(deviceResource.Instance, nullptr);

    Resources().RemoveResource(device);
}

void Driver::DeviceBuilderDefaults(Device::Builder::CreateInfo& createInfo)
{
    createInfo.AppName = "Vulkan-app";
    createInfo.APIVersion = VK_API_VERSION_1_3;
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
}

void Driver::Destroy(ResourceHandle<QueueInfo> queue)
{
    Resources().RemoveResource(queue);
}

Swapchain Driver::Create(const Swapchain::Builder::CreateInfo& createInfo)
{
    std::vector<VkSurfaceFormatKHR> desiredFormats = {{{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    std::vector<VkPresentModeKHR> desiredPresentModes = {{
            VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR}};
    
    DeviceSurfaceDetails surfaceDetails = GetSurfaceDetails(*createInfo.Device);
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
    
    const DriverResources::DeviceResource& deviceResource = Resources()[*createInfo.Device]; 
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = deviceResource.Surface;
    swapchainCreateInfo.imageColorSpace = colorFormat.colorSpace;
    swapchainCreateInfo.imageFormat = colorFormat.format;
    VkExtent2D extent = chooseExtent(createInfo.Device->m_Window, capabilities);
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.presentMode = presentMode;

    if (createInfo.Device->GetQueues().Graphics.Family == createInfo.Device->GetQueues().Presentation.Family)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        std::vector<u32> queueFamilies = createInfo.Device->GetQueues().AsFamilySet();
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = (u32)queueFamilies.size();
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();
    }
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    DriverResources::SwapchainResource swapchainResource = {};
    swapchainResource.ColorFormat = colorFormat.format;
    DriverCheck(vkCreateSwapchainKHR(DeviceHandle(), &swapchainCreateInfo, nullptr, &swapchainResource.Swapchain),
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
    swapchain.m_SwapchainFrameSync = createInfo.FrameSyncs;
    swapchain.m_Window = createInfo.Device->m_Window;

    return swapchain;
}

void Driver::Destroy(ResourceHandle<Swapchain> swapchain)
{
    vkDestroySwapchainKHR(DeviceHandle(), Resources().m_Swapchains[swapchain.m_Index].Swapchain, nullptr);
    Resources().RemoveResource(swapchain);
}

Driver::DeviceSurfaceDetails Driver::GetSurfaceDetails(const Device& device)
{
    VkPhysicalDevice gpu = Resources()[device].GPU;
    VkSurfaceKHR surface = Resources()[device].Surface;
    
    DeviceSurfaceDetails details = {};
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

std::vector<Image> Driver::CreateSwapchainImages(const Swapchain& swapchain)
{
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(DeviceHandle(), Resources()[swapchain].Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(DeviceHandle(), Resources()[swapchain].Swapchain, &imageCount, images.data());

    ImageDescription description = {
        .Width = swapchain.m_SwapchainResolution.x,
        .Height = swapchain.m_SwapchainResolution.y,
        .Layers = 1,
        .Mipmaps = 1,
        .Kind = ImageKind::Image2d,
        .Usage = ImageUsage::Destination};
    std::vector<Image> colorImages(imageCount);
    for (auto& image : colorImages)
        image.m_Description = description;
    
    
    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        DriverResources::ImageResource imageResource = {.Image = images[i]};
        colorImages[i].m_ResourceHandle = Resources().AddResource(imageResource);
        Resources()[colorImages[i]].Views.ViewType.View = CreateVulkanImageView(
            colorImages[i].Subresource(0, 1, 0, 1), Resources()[swapchain].ColorFormat);
        Resources()[colorImages[i]].Views.ViewList = &Resources()[colorImages[i]].Views.ViewType.View;
    }

    return colorImages;
}

void Driver::DestroySwapchainImages(const Swapchain& swapchain)
{
    for (const auto& colorImage : swapchain.m_ColorImages)
    {
        vkDestroyImageView(DeviceHandle(), *Resources()[colorImage].Views.ViewList, nullptr);
        Resources().RemoveResource(colorImage.Handle());
    }
    Image::Destroy(swapchain.m_DrawImage);
    Image::Destroy(swapchain.m_DepthImage);
}


CommandBuffer Driver::Create(const CommandBuffer::Builder::CreateInfo& createInfo)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = Resources()[*createInfo.Pool].CommandPool;
    allocateInfo.level = vulkanBufferLevelFromBufferKind(createInfo.Kind);
    allocateInfo.commandBufferCount = 1;

    DriverResources::CommandBufferResource commandBufferResource = {};
    DriverCheck(vkAllocateCommandBuffers(DeviceHandle(), &allocateInfo, &commandBufferResource.CommandBuffer),
        "Failed to allocate command buffer");
    
    CommandBuffer cmd = {};
    cmd.m_Kind = createInfo.Kind;
    cmd.m_ResourceHandle = Resources().AddResource(commandBufferResource);
    Resources().MapCmdToPool(cmd, *createInfo.Pool);
    
    return cmd;
}

CommandPool Driver::Create(const CommandPool::Builder::CreateInfo& createInfo)
{
    VkCommandPoolCreateFlags flags = 0;
    if (createInfo.PerBufferReset)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = flags;
    poolCreateInfo.queueFamilyIndex = createInfo.QueueFamily;

    DriverResources::CommandPoolResource commandPoolResource = {};
    DriverCheck(vkCreateCommandPool(DeviceHandle(), &poolCreateInfo, nullptr, &commandPoolResource.CommandPool),
        "Failed to create command pool");
    
    CommandPool commandPool = {};
    commandPool.m_ResourceHandle = Resources().AddResource(commandPoolResource);
    if (commandPool.Handle().m_Index >= Resources().m_CommandPoolToBuffersMap.size())
        Resources().m_CommandPoolToBuffersMap.resize(commandPool.Handle().m_Index + 1);
    
    return commandPool;
}

void Driver::Destroy(ResourceHandle<CommandPool> commandPool)
{
    vkDestroyCommandPool(DeviceHandle(), Resources().m_CommandPools[commandPool.m_Index].CommandPool, nullptr);
    Resources().DestroyCmdsOfPool(commandPool);
    Resources().RemoveResource(commandPool);
}

Buffer Driver::Create(const Buffer::Builder::CreateInfo& createInfo)
{
    VmaAllocationCreateFlags flags = 0;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::Upload))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(createInfo.Description.Usage, BufferUsage::UploadRandomAccess | BufferUsage::Readback))
        flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    if (createInfo.CreateMapped)
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    DriverResources::BufferResource bufferResource = CreateBufferResource(createInfo.Description.SizeBytes,
        vulkanBufferUsageFromUsage(createInfo.Description.Usage), flags);

    Buffer buffer = {};
    buffer.m_Description = createInfo.Description;
    if (createInfo.CreateMapped)
        buffer.m_HostAddress = bufferResource.Allocation->GetMappedData();
    buffer.m_ResourceHandle = Resources().AddResource(bufferResource);    
    return buffer;
}

void Driver::Destroy(ResourceHandle<Buffer> buffer)
{
    const DriverResources::BufferResource& resource = Resources().m_Buffers[buffer.m_Index];
    vmaDestroyBuffer(Allocator(), resource.Buffer, resource.Allocation);
    Resources().RemoveResource(buffer);
}

void* Driver::MapBuffer(const Buffer& buffer)
{
    const DriverResources::BufferResource& resource = Resources()[buffer];
    void* mappedData;
    vmaMapMemory(Allocator(), resource.Allocation, &mappedData);
    return mappedData;
}

void Driver::UnmapBuffer(const Buffer& buffer)
{
    const DriverResources::BufferResource& resource = Resources()[buffer];
    vmaUnmapMemory(Allocator(), resource.Allocation);
}

void Driver::SetBufferData(Buffer& buffer, const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    const DriverResources::BufferResource& resource = Resources()[buffer];
    void* mappedData = nullptr;
    vmaMapMemory(Allocator(), resource.Allocation, &mappedData);
    mappedData = (void*)((u8*)mappedData + offsetBytes);
    std::memcpy(mappedData, data, dataSizeBytes);
    vmaUnmapMemory(Allocator(), resource.Allocation);
}

void Driver::SetBufferData(void* mappedAddress, const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    mappedAddress = (void*)((u8*)mappedAddress + offsetBytes);
    std::memcpy(mappedAddress, data, dataSizeBytes);
}

Image Driver::AllocateImage(const Image::Builder::CreateInfo& createInfo)
{
    u32 depth = createInfo.Description.Kind != ImageKind::Image3d ? 1u : createInfo.Description.Layers;
    u32 layers = createInfo.Description.Kind != ImageKind::Image3d ? createInfo.Description.Layers : 1u;
    
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
    imageCreateInfo.mipLevels = createInfo.Description.Mipmaps;
    imageCreateInfo.arrayLayers = layers;
    imageCreateInfo.flags = vulkanImageFlagsFromImageKind(createInfo.Description.Kind);

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = enumHasAny(createInfo.Description.Usage,
        ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil) ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

    DriverResources::ImageResource imageResource = {};
    DriverCheck(vmaCreateImage(Allocator(), &imageCreateInfo, &allocationInfo,
        &imageResource.Image, &imageResource.Allocation, nullptr),
        "Failed to create image");
    
    Image image = {};
    image.m_Description = createInfo.Description;
    image.m_ResourceHandle = Resources().AddResource(imageResource);

    return image;
}

void Driver::Destroy(ResourceHandle<Image> image)
{
    const DriverResources::ImageResource& imageResource = Resources().m_Images[image.m_Index];
    if (imageResource.Views.ViewList == &imageResource.Views.ViewType.View)
    {
        vkDestroyImageView(DeviceHandle(), imageResource.Views.ViewType.View, nullptr);
    }
    else
    {
        for (u32 viewIndex = 0; viewIndex < imageResource.Views.ViewType.ViewCount; viewIndex++)
            vkDestroyImageView(DeviceHandle(), imageResource.Views.ViewList[viewIndex], nullptr);
        delete[] imageResource.Views.ViewList;
    }
    vmaDestroyImage(Allocator(), imageResource.Image, imageResource.Allocation);
    Resources().RemoveResource(image);
}

void Driver::CreateViews(const ImageSubresource& image,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    DriverResources::ImageResource& resource = Resources()[*image.Image];
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
            image.Image->Subresource(additionalViews[viewIndex]), viewFormat);
}

Sampler Driver::Create(const Sampler::Builder::CreateInfo& createInfo)
{
    
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = vulkanFilterFromImageFilter(createInfo.MagnificationFilter);
    samplerCreateInfo.minFilter = vulkanFilterFromImageFilter(createInfo.MinificationFilter);
    samplerCreateInfo.mipmapMode = vulkanMipmapModeFromSamplerFilter(samplerCreateInfo.minFilter);
    samplerCreateInfo.addressModeU = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.AddressMode);
    samplerCreateInfo.addressModeV = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.AddressMode);
    samplerCreateInfo.addressModeW = vulkanSamplerAddressModeFromSamplerWrapMode(createInfo.AddressMode);
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = createInfo.MaxLod;
    samplerCreateInfo.maxAnisotropy = GetAnisotropyLevel();
    samplerCreateInfo.anisotropyEnable = (u32)createInfo.WithAnisotropy;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSamplerReductionModeCreateInfo reductionModeCreateInfo = {};
    if (createInfo.ReductionMode.has_value())
    {
        reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        reductionModeCreateInfo.reductionMode = vulkanSamplerReductionModeFromSamplerReductionMode(
            *createInfo.ReductionMode);
        samplerCreateInfo.pNext = &reductionModeCreateInfo;    
    }
    
    DriverResources::SamplerResource samplerResource = {};
    DriverCheck(vkCreateSampler(DeviceHandle(), &samplerCreateInfo, nullptr, &samplerResource.Sampler),
        "Failed to create depth pyramid sampler");

    Sampler sampler = {};
    sampler.m_ResourceHandle = Resources().AddResource(samplerResource);
    return sampler;
}

void Driver::Destroy(ResourceHandle<Sampler> sampler)
{
    vkDestroySampler(DeviceHandle(), Resources().m_Samplers[sampler.m_Index].Sampler, nullptr);
    Resources().RemoveResource(sampler);
}

RenderingAttachment Driver::Create(const RenderingAttachment::Builder::CreateInfo& createInfo)
{
    DriverResources::RenderingAttachmentResource renderingAttachmentResource = {};

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
    renderingAttachmentResource.AttachmentInfo.imageView = *Resources()[*createInfo.Image].Views.ViewList;
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

void Driver::Destroy(ResourceHandle<RenderingAttachment> renderingAttachment)
{
    Resources().RemoveResource(renderingAttachment);
}

RenderingInfo Driver::Create(const RenderingInfo::Builder::CreateInfo& createInfo)
{
    DriverResources::RenderingInfoResource renderingInfoResource = {};
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

void Driver::Destroy(ResourceHandle<RenderingInfo> renderingInfo)
{
    Resources().RemoveResource(renderingInfo);
}

ShaderModule Driver::Create(const ShaderModule::Builder::CreateInfo& createInfo)
{
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = createInfo.Source->size();
    moduleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.Source->data());

    DriverResources::ShaderModuleResource shaderModuleResource = {};
    DriverCheck(vkCreateShaderModule(DeviceHandle(), &moduleCreateInfo, nullptr, &shaderModuleResource.Shader),
        "Failed to create shader module");

    ShaderModule shaderModule = {};
    shaderModule.m_Stage = createInfo.Stage;
    shaderModule.m_ResourceHandle = Resources().AddResource(shaderModuleResource);

    return shaderModule;
}

void Driver::Destroy(ResourceHandle<ShaderModule> shader)
{
    vkDestroyShaderModule(DeviceHandle(), Resources().m_Shaders[shader.m_Index].Shader, nullptr);
    Resources().RemoveResource(shader);
}

PipelineLayout Driver::Create(const PipelineLayout::Builder::CreateInfo& createInfo)
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

    DriverResources::PipelineLayoutResource pipelineLayoutResource = {};
    pipelineLayoutResource.PushConstants = pushConstantRanges;
    DriverCheck(vkCreatePipelineLayout(DeviceHandle(), &layoutCreateInfo, nullptr, &pipelineLayoutResource.Layout),
        "Failed to create pipeline layout");

    PipelineLayout layout = {};
    layout.m_ResourceHandle = Resources().AddResource(pipelineLayoutResource);

    return layout;
}

void Driver::Destroy(ResourceHandle<PipelineLayout> pipelineLayout)
{
    vkDestroyPipelineLayout(DeviceHandle(), Resources().m_PipelineLayouts[pipelineLayout.m_Index].Layout, nullptr);
    Resources().RemoveResource(pipelineLayout);
}

Pipeline Driver::Create(const Pipeline::Builder::CreateInfo& createInfo)
{
    VkPipelineLayout layout = Resources()[createInfo.PipelineLayout].Layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    shaders.reserve(createInfo.Shaders.size());
    for (auto& shader : createInfo.Shaders)
    {
        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.module = Resources()[shader].Shader;
        shaderStageCreateInfo.stage = vulkanStageBitFromShaderStage(shader.m_Stage);
        shaderStageCreateInfo.pName = "main";

        shaders.push_back(shaderStageCreateInfo);
    }
    
    std::vector<VkSpecializationMapEntry> shaderSpecializationEntries;
    std::vector<VkSpecializationInfo> shaderSpecializationInfos;
    shaderSpecializationInfos.reserve(createInfo.Shaders.size());
    u32 entriesOffset = 0;
    for (u32 shaderIndex = 0; shaderIndex < createInfo.Shaders.size(); shaderIndex++)
    {
        auto& shader = shaders[shaderIndex];
        VkSpecializationInfo shaderSpecializationInfo = {};
        for (const auto& specialization : createInfo.ShaderSpecialization.ShaderSpecializations)
            if (enumHasAny(createInfo.Shaders[shaderIndex].m_Stage, specialization.ShaderStages))
                shaderSpecializationEntries.push_back({
                    .constantID = specialization.Id,
                    .offset = specialization.Offset,
                    .size = specialization.SizeBytes});

        shaderSpecializationInfo.dataSize = createInfo.ShaderSpecialization.Buffer.size();
        shaderSpecializationInfo.pData = createInfo.ShaderSpecialization.Buffer.data();
        shaderSpecializationInfo.mapEntryCount = (u32)shaderSpecializationEntries.size() - entriesOffset;
        shaderSpecializationInfo.pMapEntries = shaderSpecializationEntries.data() + entriesOffset;

        shaderSpecializationInfos.push_back(shaderSpecializationInfo);
        shader.pSpecializationInfo = &shaderSpecializationInfos.back();

        entriesOffset = (u32)shaderSpecializationEntries.size();
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

        DriverResources::PipelineResource pipelineResource = {};
        DriverCheck(vkCreateComputePipelines(DeviceHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create compute pipeline");
        pipeline.m_ResourceHandle = Resources().AddResource(pipelineResource);
    }
    else
    {
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        
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
        rasterizationState.depthClampEnable = VK_FALSE;
        rasterizationState.depthBiasEnable = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0f;
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        
        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.sampleShadingEnable = VK_FALSE;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
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
        colorFormats.resize(createInfo.RenderingDetails.ColorFormats.size());

        for (u32 colorIndex = 0; colorIndex < createInfo.RenderingDetails.ColorFormats.size(); colorIndex++)
            colorFormats[colorIndex] = vulkanFormatFromFormat(createInfo.RenderingDetails.ColorFormats[colorIndex]);
        
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = (u32)colorFormats.size();
        pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats.data();
        pipelineRenderingCreateInfo.depthAttachmentFormat = vulkanFormatFromFormat(
            createInfo.RenderingDetails.DepthFormat);

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

        DriverResources::PipelineResource pipelineResource = {};
        DriverCheck(vkCreateGraphicsPipelines(DeviceHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
            &pipelineResource.Pipeline), "Failed to create graphics pipeline");
        pipeline.m_ResourceHandle = Resources().AddResource(pipelineResource);
    }
    
    return pipeline;
}

void Driver::Destroy(ResourceHandle<Pipeline> pipeline)
{
    vkDestroyPipeline(DeviceHandle(), Resources().m_Pipelines[pipeline.m_Index].Pipeline, nullptr);
    Resources().RemoveResource(pipeline);
}

DescriptorsLayout Driver::Create(const DescriptorsLayout::Builder::CreateInfo& createInfo)
{
    static Sampler immutableSampler = GetImmutableSampler(ImageFilter::Linear, SamplerWrapMode::Repeat);
    static Sampler immutableSamplerNearest = GetImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::Repeat);
    static Sampler immutableSamplerClampEdge =
        GetImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampEdge);
    static Sampler immutableSamplerNearestClampEdge =
        GetImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampEdge);
    
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
            .stageFlags = vulkanShaderStageFromShaderStage(binding.Shaders)});

        if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderInfo::DescriptorSet::ImmutableSamplerClampEdge))
            bindings.back().pImmutableSamplers = &Resources()[immutableSamplerClampEdge].Sampler;
        else if (enumHasAny(binding.DescriptorFlags,
            assetLib::ShaderInfo::DescriptorSet::ImmutableSamplerNearestClampEdge))
                bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearestClampEdge].Sampler;
        else if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderInfo::DescriptorSet::ImmutableSamplerNearest))
            bindings.back().pImmutableSamplers = &Resources()[immutableSamplerNearest].Sampler;
        else if (enumHasAny(binding.DescriptorFlags, assetLib::ShaderInfo::DescriptorSet::ImmutableSampler))
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

    DriverResources::DescriptorSetLayoutResource descriptorSetLayoutResource = {};
    DriverCheck(vkCreateDescriptorSetLayout(DeviceHandle(), &layoutCreateInfo, nullptr,
        &descriptorSetLayoutResource.Layout), "Failed to create descriptor set layout");
    
    DescriptorsLayout layout = {};
    layout.m_ResourceHandle = Resources().AddResource(descriptorSetLayoutResource);

    return layout;
}

void Driver::Destroy(ResourceHandle<DescriptorsLayout> layout)
{
    vkDestroyDescriptorSetLayout(DeviceHandle(), Resources().m_DescriptorLayouts[layout.m_Index].Layout, nullptr);
    Resources().RemoveResource(layout);
}

DescriptorSet Driver::Create(const DescriptorSet::Builder::CreateInfo& createInfo)
{
    // prepare 'bindless' descriptors info
    std::vector<DescriptorSet::Builder::VariableBindingInfo> variableBindingInfos(
        createInfo.VariableBindingSlots.size());
    for (u32 i = 0; i < variableBindingInfos.size(); i++)
        variableBindingInfos[i] = {
            .Slot = createInfo.VariableBindingSlots[i],
            .Count = createInfo.VariableBindingCounts[i]};
    std::ranges::sort(variableBindingInfos,
        [](u32 a, u32 b) { return a < b; },
        [](const DescriptorSet::Builder::VariableBindingInfo& v) { return v.Slot; });
    std::vector<u32> variableBindingCounts(createInfo.VariableBindingCounts.size());
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
        
        const DriverResources::BufferResource& bufferResource = Resources()[*buffer.BindingInfo.Buffer];
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
        descriptorTextureInfo.imageView = binding.ViewHandle.m_Index == ImageViewHandle::NON_INDEX ?
            *Resources()[*binding.Image].Views.ViewList :
            Resources()[*binding.Image].Views.ViewList[binding.ViewHandle.m_Index];
        descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(binding.Layout);
        boundTextures.push_back(descriptorTextureInfo);
        write.pImageInfo = &boundTextures.back();
        writes.push_back(write);
    }
    
    vkUpdateDescriptorSets(DeviceHandle(), (u32)writes.size(), writes.data(), 0, nullptr);

    return descriptorSet;
}

void Driver::AllocateDescriptorSet(DescriptorAllocator& allocator, DescriptorSet& set, DescriptorPoolFlags poolFlags,
        const std::vector<u32>& variableBindingCounts)
{
    DriverResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
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

    DriverResources::DescriptorSetResource descriptorSetResource = {};
    vkAllocateDescriptorSets(DeviceHandle(), &allocateInfo, &descriptorSetResource.DescriptorSet);
    descriptorSetResource.Pool = pool;

    if (descriptorSetResource.DescriptorSet == VK_NULL_HANDLE)
    {
        allocatorResource.UsedPools.push_back(allocatorResource.FreePools[poolIndex]);
        allocatorResource.FreePools.erase(allocatorResource.FreePools.begin() + poolIndex);

        poolIndex = GetFreePoolIndexFromAllocator(allocator, poolFlags);
        pool = allocatorResource.FreePools[poolIndex].Pool;
        allocateInfo.descriptorPool = pool;
        allocateInfo.pSetLayouts = &Resources()[set.m_Layout].Layout;
        DriverCheck(vkAllocateDescriptorSets(DeviceHandle(), &allocateInfo, &descriptorSetResource.DescriptorSet),
            "Failed to allocate descriptor set");
        descriptorSetResource.Pool = pool;
    }
    allocatorResource.FreePools[poolIndex].AllocationCount++;

    set.m_ResourceHandle = Resources().AddResource(descriptorSetResource);
}

void Driver::DeallocateDescriptorSet(ResourceHandle<DescriptorAllocator> allocator, ResourceHandle<DescriptorSet> set)
{
    DriverResources::DescriptorAllocatorResource& allocatorResource =
        Resources().m_DescriptorAllocators[allocator.m_Index];
    VkDescriptorPool pool = Resources().m_DescriptorSets[set.m_Index].Pool;

    auto it = std::ranges::find(allocatorResource.FreePools, pool,
        [](const DriverResources::DescriptorAllocatorResource::PoolInfo& info)
        {
            return info.Pool;
        });
    if (it == allocatorResource.FreePools.end())
    {
        it = std::ranges::find(allocatorResource.UsedPools, pool,
            [](const DriverResources::DescriptorAllocatorResource::PoolInfo& info)
            {
                return info.Pool;
            });
        ASSERT(it != allocatorResource.UsedPools.end(), "Descriptor set wasn't allocated with this allocator")
        it->AllocationCount--;
        allocatorResource.FreePools.push_back(*it);
        allocatorResource.UsedPools.erase(it);
    }
    
    vkFreeDescriptorSets(DeviceHandle(), pool, 1, &Resources().m_DescriptorSets[set.m_Index].DescriptorSet);
    Resources().RemoveResource(set);
}

void Driver::UpdateDescriptorSet(DescriptorSet& descriptorSet,
    u32 slot, const Texture& texture, DescriptorType type, u32 arrayIndex)
{
    ImageBindingInfo bindingInfo = texture.BindingInfo({}, ImageLayout::Readonly);
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = Resources()[bindingInfo.Sampler].Sampler;
    descriptorTextureInfo.imageView = bindingInfo.ViewHandle.m_Index == ImageViewHandle::NON_INDEX ?
        *Resources()[*bindingInfo.Image].Views.ViewList :
        Resources()[*bindingInfo.Image].Views.ViewList[bindingInfo.ViewHandle.m_Index];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(bindingInfo.Layout);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = Resources()[descriptorSet].DescriptorSet;
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = arrayIndex;
    write.descriptorType = vulkanDescriptorTypeFromDescriptorType(type);

    vkUpdateDescriptorSets(DeviceHandle(), 1, &write, 0, nullptr);
}

DescriptorAllocator Driver::Create(const DescriptorAllocator::Builder::CreateInfo& createInfo)
{
    DriverResources::DescriptorAllocatorResource descriptorAllocatorResource = {};
    
    DescriptorAllocator allocator = {};
    allocator.m_MaxSetsPerPool = createInfo.MaxSets;
    allocator.m_ResourceHandle = Resources().AddResource(descriptorAllocatorResource);
    if (allocator.Handle().m_Index >= Resources().m_DescriptorAllocatorToSetsMap.size())
        Resources().m_DescriptorAllocatorToSetsMap.resize(allocator.Handle().m_Index + 1);

    return allocator;
}

void Driver::Destroy(ResourceHandle<DescriptorAllocator> allocator)
{
    DriverResources::DescriptorAllocatorResource& allocatorResource =
        Resources().m_DescriptorAllocators[allocator.m_Index];
    for (auto& pool : allocatorResource.FreePools)
        vkDestroyDescriptorPool(DeviceHandle(), pool.Pool, nullptr);
    for (auto& pool : allocatorResource.UsedPools)
        vkDestroyDescriptorPool(DeviceHandle(), pool.Pool, nullptr);

    Resources().DestroyDescriptorSetsOfAllocator(allocator);
    Resources().RemoveResource(allocator);
}

void Driver::ResetAllocator(DescriptorAllocator& allocator)
{
    DriverResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
    for (auto& pool : allocatorResource.FreePools)
        vkResetDescriptorPool(DeviceHandle(), pool.Pool, 0);
    for (auto pool : allocatorResource.UsedPools)
    {
        vkResetDescriptorPool(DeviceHandle(), pool.Pool, 0);
        allocatorResource.FreePools.push_back(pool);
    }
    allocatorResource.UsedPools.clear();
    Resources().DestroyDescriptorSetsOfAllocator(allocator.Handle());
}

DescriptorArenaAllocator Driver::Create(const DescriptorArenaAllocator::Builder::CreateInfo& createInfo)
{
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

    DriverResources::BufferResource arenaResource = CreateBufferResource(arenaSizeBytes,
        usageFlags, allocationFlags);
    Buffer arena = {};
    arena.m_Description = {
        .SizeBytes = arenaSizeBytes,
        .Usage = bufferUsage};
    arena.m_HostAddress = arenaResource.Allocation->GetMappedData();
    arena.m_ResourceHandle = Resources().AddResource(arenaResource);
    

    DescriptorArenaAllocator descriptorArenaAllocator = {};
    descriptorArenaAllocator.m_Kind = createInfo.Kind;
    descriptorArenaAllocator.m_Residence = createInfo.Residence;
    descriptorArenaAllocator.m_UsedTypes = createInfo.UsedTypes;
    descriptorArenaAllocator.m_Buffer = arena;

    return descriptorArenaAllocator;
}

std::optional<Descriptors> Driver::Allocate(DescriptorArenaAllocator& allocator,
    DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings)
{
    auto& descriptorBufferProps = Resources().m_Devices[0].GPUDescriptorBufferProperties;


    // if we have bindless binding, we have to calculate layout size as a sum of bindings sizes
    u64 layoutSizeBytes = 0;
    if (bindings.BindlessCount == 0)
    {
        vkGetDescriptorSetLayoutSizeEXT(DeviceHandle(), Resources()[layout].Layout, &layoutSizeBytes);    
    }    
    else
    {
        for (u32 bindingIndex = 0; bindingIndex < bindings.Bindings.size(); bindingIndex++)
        {
            auto& binding = bindings.Bindings[bindingIndex];
            bool isBindless = enumHasAny(binding.DescriptorFlags, assetLib::ShaderInfo::DescriptorSet::Bindless);
            ASSERT(
                (bindingIndex == (u32)bindings.Bindings.size() - 1 && isBindless) ||
                (bindingIndex != (u32)bindings.Bindings.size() - 1 && !isBindless),
                "Only one binding can be declared as 'bindless' for any particular set, and it has to be the last one")

            layoutSizeBytes += isBindless ? bindings.BindlessCount * GetDescriptorSizeBytes(binding.Type) :
                GetDescriptorSizeBytes(binding.Type);
        }
    }
    
    layoutSizeBytes = CoreUtils::align(layoutSizeBytes, descriptorBufferProps.descriptorBufferOffsetAlignment);
    if (layoutSizeBytes + allocator.m_CurrentOffset > allocator.m_Buffer.GetSizeBytes())
        return {};

    std::vector<u64> bindingOffsets(bindings.Bindings.size());
    for (u32 offsetIndex = 0; offsetIndex < bindingOffsets.size(); offsetIndex++)
    {
        auto& binding = bindings.Bindings[offsetIndex];
        vkGetDescriptorSetLayoutBindingOffsetEXT(DeviceHandle(), Resources()[layout].Layout, binding.Binding,
            &bindingOffsets[offsetIndex]);
        bindingOffsets[offsetIndex] += allocator.m_CurrentOffset;
    }
    
    Descriptors descriptors = {};
    descriptors.m_Offsets = bindingOffsets;
    descriptors.m_Allocator = &allocator;

    allocator.m_CurrentOffset += layoutSizeBytes;
    
    return descriptors;
}

void Driver::UpdateDescriptors(const Descriptors& descriptors, u32 slot, const BufferBindingInfo& buffer,
    DescriptorType type)
{
    ASSERT(type != DescriptorType::TexelStorage && type != DescriptorType::TexelUniform,
        "Texel buffers require format information")
    ASSERT(type != DescriptorType::StorageBufferDynamic && type != DescriptorType::UniformBufferDynamic,
        "Dynamic buffers are not supported when using descriptor buffer")
    
    VkBufferDeviceAddressInfo deviceAddressInfo = {};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = Resources()[*buffer.Buffer].Buffer;
    u64 deviceAddress = vkGetBufferDeviceAddress(DeviceHandle(), &deviceAddressInfo);

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

    vkGetDescriptorEXT(DeviceHandle(), &descriptorGetInfo, GetDescriptorSizeBytes(type),
        (u8*)descriptors.m_Allocator->m_Buffer.m_HostAddress + descriptors.m_Offsets[slot]);
}

void Driver::UpdateDescriptors(const Descriptors& descriptors, u32 slot, const TextureBindingInfo& texture,
    DescriptorType type, u32 bindlessIndex)
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
        descriptorImageInfo.sampler = Resources()[texture.Sampler].Sampler;
        descriptorImageInfo.imageView = texture.ViewHandle.m_Index == ImageViewHandle::NON_INDEX ?
            *Resources()[*texture.Image].Views.ViewList :
            Resources()[*texture.Image].Views.ViewList[texture.ViewHandle.m_Index];
        descriptorImageInfo.imageLayout = vulkanImageLayoutFromImageLayout(texture.Layout);
        descriptorGetInfo.data.pSampledImage = &descriptorImageInfo;
    }

    u64 descriptorSizeBytes = GetDescriptorSizeBytes(type);
    u64 offsetBytes = descriptors.m_Offsets[slot] + descriptorSizeBytes * bindlessIndex;
    vkGetDescriptorEXT(DeviceHandle(), &descriptorGetInfo, descriptorSizeBytes,
        (u8*)descriptors.m_Allocator->m_Buffer.m_HostAddress + offsetBytes);
}

u32 Driver::GetDescriptorSizeBytes(DescriptorType type)
{
    auto& props = Resources().m_Devices[0].GPUDescriptorBufferProperties;
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

DriverResources::BufferResource Driver::CreateBufferResource(u64 sizeBytes, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags allocationFlags)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = sizeBytes;
    bufferCreateInfo.usage = usage;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = allocationFlags;

    DriverResources::BufferResource bufferResource = {};
    DriverCheck(vmaCreateBuffer(Allocator(), &bufferCreateInfo, &allocationCreateInfo,
        &bufferResource.Buffer, &bufferResource.Allocation, nullptr),
        "Failed to create a buffer");

    return bufferResource;
}

Fence Driver::Create(const Fence::Builder::CreateInfo& createInfo)
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (createInfo.IsSignaled)
        fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        fenceCreateInfo.flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    DriverResources::FenceResource fenceResource = {};    
    DriverCheck(vkCreateFence(DeviceHandle(), &fenceCreateInfo, nullptr, &fenceResource.Fence),
        "Failed to create fence");
    
    Fence fence = {};
    fence.m_ResourceHandle = Resources().AddResource(fenceResource);

    return fence;
}

void Driver::Destroy(ResourceHandle<Fence> fence)
{
    vkDestroyFence(DeviceHandle(), Resources().m_Fences[fence.m_Index].Fence, nullptr);
    Resources().RemoveResource(fence);
}

Semaphore Driver::Create(const Semaphore::Builder::CreateInfo& createInfo)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    DriverResources::SemaphoreResource semaphoreResource = {};
    DriverCheck(vkCreateSemaphore(DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore),
        "Failed to create semaphore");
    
    Semaphore semaphore = {};
    semaphore.m_ResourceHandle = Resources().AddResource(semaphoreResource);
    
    return semaphore;
}

void Driver::Destroy(ResourceHandle<Semaphore> semaphore)
{
    vkDestroySemaphore(DeviceHandle(), Resources().m_Semaphores[semaphore.m_Index].Semaphore, nullptr);
    Resources().RemoveResource(semaphore);
}

TimelineSemaphore Driver::Create(const TimelineSemaphore::Builder::CreateInfo& createInfo)
{
    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = createInfo.InitialValue;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    DriverResources::SemaphoreResource semaphoreResource = {};
    vkCreateSemaphore(DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphoreResource.Semaphore);

    TimelineSemaphore semaphore = {};
    semaphore.m_Timeline = createInfo.InitialValue;
    semaphore.m_ResourceHandle = Resources().AddResource(semaphoreResource);
    
    return semaphore;
}

void Driver::Destroy(ResourceHandle<TimelineSemaphore> semaphore)
{
    vkDestroySemaphore(DeviceHandle(), Resources().m_Semaphores[semaphore.m_Index].Semaphore, nullptr);
    Resources().RemoveResource(semaphore);
}

void Driver::TimelineSemaphoreWaitCPU(const TimelineSemaphore& semaphore, u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &Resources()[semaphore].Semaphore;
    waitInfo.pValues = &value;
    
    DriverCheck(vkWaitSemaphores(DeviceHandle(), &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void Driver::TimelineSemaphoreSignalCPU(TimelineSemaphore& semaphore, u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = Resources()[semaphore].Semaphore;
    signalInfo.value = value;

    DriverCheck(vkSignalSemaphore(DeviceHandle(), &signalInfo),
        "Failed to signal semaphore");

    semaphore.m_Timeline = value;
}

SplitBarrier Driver::Create(const SplitBarrier::Builder::CreateInfo& createInfo)
{
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    DriverResources::SplitBarrierResource splitBarrierResource = {};
    DriverCheck(vkCreateEvent(DeviceHandle(), &eventCreateInfo, nullptr, &splitBarrierResource.Event),
        "Failed to create split barrier");

    SplitBarrier splitBarrier = {};
    splitBarrier.m_ResourceHandle = Resources().AddResource(splitBarrierResource);
    
    return splitBarrier;
    
}

void Driver::Destroy(ResourceHandle<SplitBarrier> splitBarrier)
{
    vkDestroyEvent(DeviceHandle(), Resources().m_SplitBarriers[splitBarrier.m_Index].Event, nullptr);
    Resources().RemoveResource(splitBarrier);
}

DependencyInfo Driver::Create(const DependencyInfo::Builder::CreateInfo& createInfo)
{
    DriverResources::DependencyInfoResource dependencyInfoResource = {};
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
            .baseMipLevel = createInfo.LayoutTransitionInfo->ImageSubresource.Description.MipmapBase,
            .levelCount = createInfo.LayoutTransitionInfo->ImageSubresource.Description.Mipmaps,
            .baseArrayLayer = createInfo.LayoutTransitionInfo->ImageSubresource.Description.LayerBase,
            .layerCount = createInfo.LayoutTransitionInfo->ImageSubresource.Description.Layers};

        dependencyInfoResource.LayoutTransitionsInfo.push_back(imageMemoryBarrier);
    }

    DependencyInfo dependencyInfo = {};
    dependencyInfo.m_ResourceHandle = Resources().AddResource(dependencyInfoResource);
    
    return dependencyInfo;
}

void Driver::Destroy(ResourceHandle<DependencyInfo> dependencyInfo)
{
    Resources().RemoveResource(dependencyInfo);
}

u32 Driver::GetFreePoolIndexFromAllocator(DescriptorAllocator& allocator, DescriptorPoolFlags poolFlags)
{
    DriverResources::DescriptorAllocatorResource& allocatorResource = Resources()[allocator];
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

    DriverCheck(vkCreateDescriptorPool(DeviceHandle(), &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    allocatorResource.FreePools.push_back({.Pool = pool, .Flags = poolFlags});

    return index;
}

void Driver::CreateInstance(const Device::Builder::CreateInfo& createInfo,
                            DriverResources::DeviceResource& deviceResource)
{
    auto checkInstanceExtensions = [](const Device::Builder::CreateInfo& createInfo)
    {
        u32 availableExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

        return utils::checkArrayContainsSubArray(createInfo.InstanceExtensions, availableExtensions,
            [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
            [](const char* req) { LOG("Unsupported instance extension: {}\n", req); });
    };
    auto checkInstanceValidationLayers = [](const Device::Builder::CreateInfo& createInfo)
    {
        u32 availableValidationLayerCount = 0;
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(availableValidationLayerCount);
        vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, availableLayers.data());

        return utils::checkArrayContainsSubArray(createInfo.InstanceValidationLayers, availableLayers,
            [](const char* req, const VkLayerProperties& avail) { return std::strcmp(req, avail.layerName); },
            [](const char* req) { LOG("Unsupported validation layer: {}\n", req); });
    };
    
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = createInfo.AppName.data();
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.pEngineName = "No engine";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.apiVersion = createInfo.APIVersion;

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
    DriverCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &deviceResource.Instance),
        "Failed to create instance\n");

    volkLoadInstance(deviceResource.Instance);
}

void Driver::CreateSurface(const Device::Builder::CreateInfo& createInfo,
    DriverResources::DeviceResource& deviceResource, Device& device)
{
    ASSERT(createInfo.Window != nullptr, "Window pointer is unset")
    device.m_Window = createInfo.Window;
    DriverCheck(glfwCreateWindowSurface(deviceResource.Instance, createInfo.Window, nullptr, &deviceResource.Surface),
        "Failed to create surface\n");
}

void Driver::ChooseGPU(const Device::Builder::CreateInfo& createInfo,
    DriverResources::DeviceResource& deviceResource, Device& device)
{
    auto findQueueFamilies = [&deviceResource](VkPhysicalDevice gpu, bool dedicatedCompute)
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
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, deviceResource.Surface, &isPresentationSupported);
            if (isPresentationSupported && queues.Presentation.Family == QueueInfo::UNSET_FAMILY)
                queues.Presentation.Family = i;

            if (queues.IsComplete())
                break;
        }

        return queues;
    };
    
    auto isGPUSuitable = [&deviceResource, &findQueueFamilies](VkPhysicalDevice gpu,
        const Device::Builder::CreateInfo& createInfo)
    {
        auto checkGPUExtensions = [](VkPhysicalDevice gpu, const Device::Builder::CreateInfo& createInfo)
        {
            u32 availableExtensionCount = 0;
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, availableExtensions.data());

            return utils::checkArrayContainsSubArray(createInfo.DeviceExtensions, availableExtensions,
                [](const char* req, const VkExtensionProperties& avail)
                {
                    return std::strcmp(req, avail.extensionName);
                },
                [](const char* req) { LOG("Unsupported device extension: {}\n", req); });
        };

        auto getSurfaceDetails = [](VkPhysicalDevice gpu, VkSurfaceKHR surface)
        {
            DeviceSurfaceDetails details = {};
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
                vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount,
                    details.PresentModes.data());
            }
    
            return details;
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
                    deviceVulkan12Features.shaderInt8 == VK_TRUE &&
                    deviceVulkan12Features.storageBuffer8BitAccess  == VK_TRUE &&
                    deviceVulkan12Features.uniformAndStorageBuffer8BitAccess == VK_TRUE &&
                    deviceVulkan12Features.shaderBufferInt64Atomics == VK_TRUE &&
                    deviceVulkan12Features.timelineSemaphore == VK_TRUE &&
                    deviceVulkan12Features.bufferDeviceAddress == VK_TRUE &&
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
    
        DeviceSurfaceDetails surfaceDetails = getSurfaceDetails(gpu, deviceResource.Surface);
        if (!surfaceDetails.IsSufficient())
            return false;

        bool isEveryFeatureSupported = checkGPUFeatures(gpu);
        if (!isEveryFeatureSupported)
            return false;
    
        return true;
    };
    
    u32 availableGPUCount = 0;
    vkEnumeratePhysicalDevices(deviceResource.Instance, &availableGPUCount, nullptr);
    std::vector<VkPhysicalDevice> availableGPUs(availableGPUCount);
    vkEnumeratePhysicalDevices(deviceResource.Instance, &availableGPUCount, availableGPUs.data());

    for (auto candidate : availableGPUs)
    {
        if (isGPUSuitable(candidate, createInfo))
        {
            deviceResource.GPU = candidate;
            device.m_Queues = findQueueFamilies(candidate, createInfo.AsyncCompute);
            break;
        }
    }
    
    ASSERT(deviceResource.GPU != VK_NULL_HANDLE, "Failed to find suitable gpu device")

    deviceResource.GPUDescriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    deviceResource.GPUSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    deviceResource.GPUSubgroupProperties.pNext = &deviceResource.GPUDescriptorIndexingProperties;

    deviceResource.GPUDescriptorBufferProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    deviceResource.GPUDescriptorBufferProperties.pNext = &deviceResource.GPUSubgroupProperties;
    
    VkPhysicalDeviceProperties2 deviceProperties2 = {};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &deviceResource.GPUDescriptorBufferProperties;
    vkGetPhysicalDeviceProperties2(deviceResource.GPU, &deviceProperties2);
    deviceResource.GPUProperties = deviceProperties2.properties;
}

void Driver::CreateDevice(const Device::Builder::CreateInfo& createInfo,
    DriverResources::DeviceResource& deviceResource, Device& device)
{
    std::vector<u32> queueFamilies = device.m_Queues.AsFamilySet();
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
    vulkan12Features.shaderInt8 = VK_TRUE;
    vulkan12Features.storageBuffer8BitAccess  = VK_TRUE;
    vulkan12Features.shaderBufferInt64Atomics = VK_TRUE;
    vulkan12Features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    vulkan12Features.timelineSemaphore = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;

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

    DriverCheck(vkCreateDevice(deviceResource.GPU, &deviceCreateInfo, nullptr, &deviceResource.Device),
        "Failed to create device\n");

    volkLoadDevice(deviceResource.Device);
}

void Driver::RetrieveDeviceQueues(DriverResources::DeviceResource& deviceResource, Device& device)
{
    device.m_Queues.Graphics.m_ResourceHandle = Resources().AddResource(DriverResources::QueueResource{});
    device.m_Queues.Presentation.m_ResourceHandle = Resources().AddResource(DriverResources::QueueResource{});
    device.m_Queues.Compute.m_ResourceHandle = Resources().AddResource(DriverResources::QueueResource{});

    DeletionQueue().Enqueue(device.m_Queues.Graphics);
    DeletionQueue().Enqueue(device.m_Queues.Presentation);
    DeletionQueue().Enqueue(device.m_Queues.Compute);
    
    vkGetDeviceQueue(deviceResource.Device, device.m_Queues.Graphics.Family, 0,
        &Resources()[device.m_Queues.Graphics].Queue);
    vkGetDeviceQueue(deviceResource.Device, device.m_Queues.Presentation.Family, 0,
        &Resources()[device.m_Queues.Presentation].Queue);
    vkGetDeviceQueue(deviceResource.Device, device.m_Queues.Compute.Family, 0,
        &Resources()[device.m_Queues.Compute].Queue);
}

namespace
{
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        LOG("VALIDATION LAYER: {}", pCallbackData->pMessage);
        return VK_FALSE;
    }
}

void Driver::CreateDebugUtilsMessenger(DriverResources::DeviceResource& deviceResource)
{
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugCallback;
    vkCreateDebugUtilsMessengerEXT(
        deviceResource.Instance, &debugUtilsMessengerCreateInfo, nullptr, &deviceResource.DebugUtilsMessenger);
}

void Driver::DestroyDebugUtilsMessenger(DriverResources::DeviceResource& deviceResource)
{
    vkDestroyDebugUtilsMessengerEXT(deviceResource.Instance, deviceResource.DebugUtilsMessenger, nullptr);
}

void Driver::WaitIdle()
{
    vkDeviceWaitIdle(DeviceHandle());
}

void Driver::Init(const Device& device)
{
    s_State.Device = &device;

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
    VmaAllocatorCreateInfo createInfo = {};

    const DriverResources::DeviceResource& deviceResource = Resources()[device];
    
    createInfo.instance = deviceResource.Instance;
    createInfo.physicalDevice = deviceResource.GPU;
    createInfo.device = deviceResource.Device;
    createInfo.pVulkanFunctions = (const VmaVulkanFunctions*)&vulkanFunctions;
    createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
    vmaCreateAllocator(&createInfo, &s_State.Allocator);

    s_State.SubmitContext.CommandPool = CommandPool::Builder()
        .SetQueue(QueueKind::Graphics)
        .Build();
    s_State.SubmitContext.CommandBuffer = s_State.SubmitContext.CommandPool.AllocateBuffer(CommandBufferKind::Primary);
    s_State.SubmitContext.Fence = Fence::Builder().Build();
    s_State.SubmitContext.QueueInfo = s_State.Device->GetQueues().Graphics;

    Resources().m_Images.SetOnResizeCallback(
        [](DriverResources::ImageResource* oldMem, DriverResources::ImageResource* newMem)
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

void Driver::Shutdown()
{
    vkDeviceWaitIdle(DeviceHandle());
    s_State.DeletionQueue.Flush();
    ShutdownResources();
}

void Driver::ShutdownResources()
{
    vmaDestroyAllocator(s_State.Allocator);
        for (auto device : DeletionQueue().m_Devices)
            Destroy(device);
    DeletionQueue().m_Devices.clear();
    
    ASSERT(Resources().m_AllocatedCount == Resources().m_DeallocatedCount,
        "Not all driver resources are destroyed")
}

Sampler Driver::GetImmutableSampler(ImageFilter filter, SamplerWrapMode wrapMode)
{
    Sampler sampler = Sampler::Builder()
        .Filters(filter, filter)
        .WrapMode(wrapMode)
        .Build();

    return sampler;
}

TracyVkCtx Driver::CreateTracyGraphicsContext(const CommandBuffer& cmd)
{
    const DriverResources::DeviceResource& deviceResource = Resources()[GetDevice()];
   
    TracyVkCtx context = TracyVkContext(deviceResource.GPU, deviceResource.Device,
        Resources()[GetDevice().GetQueues().Graphics].Queue, Resources()[cmd].CommandBuffer)
    return context;
}

void Driver::DestroyTracyGraphicsContext(TracyVkCtx context)
{
    TracyVkDestroy(context)
}

VkCommandBuffer Driver::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return Resources()[*context->m_GraphicsCommandBuffers[context->m_CurrentFrame]].CommandBuffer;
}

VkImageView Driver::CreateVulkanImageView(const ImageSubresource& image, VkFormat format)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = Resources()[*image.Image].Image;
    createInfo.format = format;
    createInfo.viewType = vulkanImageViewTypeFromImageKind(image.Image->m_Description.Kind);
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = vulkanImageAspectFromImageUsage(
        image.Image->m_Description.Usage);
    createInfo.subresourceRange.baseMipLevel = image.Description.MipmapBase;
    createInfo.subresourceRange.levelCount = image.Description.Mipmaps;
    createInfo.subresourceRange.baseArrayLayer = image.Description.LayerBase;
    createInfo.subresourceRange.layerCount = image.Description.Layers;

    VkImageView imageView;

    DriverCheck(vkCreateImageView(DeviceHandle(), &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

std::pair<VkBlitImageInfo2, VkImageBlit2> Driver::CreateVulkanBlitInfo(const ImageBlitInfo& source,
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

std::pair<VkCopyImageInfo2, VkImageCopy2> Driver::CreateVulkanImageCopyInfo(const ImageCopyInfo& source,
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

VkBufferImageCopy2 Driver::CreateVulkanImageCopyInfo(const ImageSubresource& subresource)
{
    ASSERT(subresource.Description.Mipmaps == 1, "Buffer to image copies one mipmap at a time")
    
    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.imageExtent = {
        .width = subresource.Image->m_Description.Width,
        .height = subresource.Image->m_Description.Height,
        .depth = subresource.Image->m_Description.Kind == ImageKind::Image3d ?
            (i32)subresource.Image->m_Description.Layers : 1u};
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(
        subresource.Image->m_Description.Usage);
    bufferImageCopy.imageSubresource.mipLevel = subresource.Description.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = subresource.Description.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = subresource.Description.Layers;

    return bufferImageCopy;
}

std::vector<VkSemaphoreSubmitInfo> Driver::CreateVulkanSemaphoreSubmit(const std::vector<Semaphore*>& semaphores,
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

std::vector<VkSemaphoreSubmitInfo> Driver::CreateVulkanSemaphoreSubmit(
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
