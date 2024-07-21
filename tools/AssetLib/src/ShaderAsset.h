#pragma once

#include "AssetLib.h"

#include "Core/core.h"

namespace assetLib
{
    struct ShaderStageInfo : AssetInfoBase
    {
        struct SpecializationConstant
        {
            std::string Name;
            u32 Id;
            u32 ShaderStages;
        };
        struct InputAttribute
        {
            std::string Name;
            u32 Binding;
            u32 Location;
            u32 Format;
            u32 SizeBytes;
        };
        struct PushConstant
        {
            u32 SizeBytes;
            u32 Offset;
            u32 ShaderStages;
        };
        struct DescriptorSet
        {
            enum DescriptorFlags
            {
                None                                = 0,
                Dynamic                             = BIT(1),
                Bindless                            = BIT(2),
                ImmutableSampler                    = BIT(3),
                ImmutableSamplerNearest             = BIT(4),
                ImmutableSamplerClampEdge           = BIT(5),
                ImmutableSamplerNearestClampEdge    = BIT(6),
                ImmutableSamplerClampBlack          = BIT(7),
                ImmutableSamplerNearestClampBlack   = BIT(8),
                ImmutableSamplerClampWhite          = BIT(9),
                ImmutableSamplerNearestClampWhite   = BIT(10),
            };
            
            struct DescriptorBinding
            {
                std::string Name;
                u32 Count;
                u32 Binding;
                u32 Type;
                u32 ShaderStages;
                DescriptorFlags Flags{None};
            };
            u32 Set;
            std::vector<DescriptorBinding> Descriptors;
        };

        std::vector<std::string> IncludedFiles;
        
        u32 ShaderStages;
        std::vector<SpecializationConstant> SpecializationConstants;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;
        u64 SourceSizeBytes;
    };
    CREATE_ENUM_FLAGS_OPERATORS(ShaderStageInfo::DescriptorSet::DescriptorFlags)

    ShaderStageInfo readShaderStageInfo(const assetLib::File& file);

    assetLib::File packShaderStage(const ShaderStageInfo& info, const void* source);
    void unpackShaderStage(ShaderStageInfo& info, const u8* source, u64 sourceSizeBytes, u8* spirv);

    std::string descriptorFlagToString(ShaderStageInfo::DescriptorSet::DescriptorFlags flag);
}
