#pragma once

#include "AssetLib.h"

#include <vulkan/vulkan_core.h>
#include "Core/core.h"

namespace assetLib
{
    struct ShaderInfo : AssetInfoBase
    {
        struct SpecializationConstant
        {
            u32 Id;
            std::string Name;
            VkShaderStageFlags ShaderStages;
        };
        struct InputAttribute
        {
            u32 Binding;
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
            enum DescriptorFlags
            {
                None = 0,
                Dynamic = BIT(1),
                Bindless = BIT(2),
                ImmutableSampler = BIT(3)
            };
            
            struct DescriptorBinding
            {
                u32 Binding;
                std::string Name;
                VkDescriptorType Type;
                VkShaderStageFlags ShaderStages;
                DescriptorFlags Flags{None};
            };
            u32 Set;
            std::vector<DescriptorBinding> Descriptors;
        };

        VkShaderStageFlags ShaderStages;
        std::vector<SpecializationConstant> SpecializationConstants;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;
        u64 SourceSizeBytes;
    };
    CREATE_ENUM_FLAGS_OPERATORS(ShaderInfo::DescriptorSet::DescriptorFlags)

    ShaderInfo readShaderInfo(const assetLib::File& file);

    assetLib::File packShader(const ShaderInfo& info, const void* source);
    void unpackShader(ShaderInfo& info, const u8* source, u64 sourceSizeBytes, u8* spirv);

    std::string descriptorFlagToString(ShaderInfo::DescriptorSet::DescriptorFlags flag);
}
