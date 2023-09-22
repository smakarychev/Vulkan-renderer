#pragma once

#include "AssetLib.h"

#include <vulkan/vulkan_core.h>

namespace assetLib
{
    struct ShaderInfo : AssetInfoBase
    {
        struct InputAttribute
        {
            u32 Location;
            std::string Name;
            VkFormat Format;
        };
        struct PushConstant
        {
            u32 SizeBytes;
            u32 Offset;
            VkShaderStageFlags ShaderStages;
        };
        struct DescriptorSet
        {
            struct DescriptorBinding
            {
                u32 Binding;
                std::string Name;
                VkDescriptorType Descriptor;
                VkShaderStageFlags ShaderStages;
            };
            u32 Set;
            std::vector<DescriptorBinding> Bindings;
        };

        VkShaderStageFlags ShaderStages;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;
        u64 SourceSizeBytes;
    };

    ShaderInfo readShaderInfo(const assetLib::File& file);

    assetLib::File packShader(const ShaderInfo& info, void* source);
    void unpackShader(ShaderInfo& info, const u8* source, u64 sourceSizeBytes, u8* spirv);
}
