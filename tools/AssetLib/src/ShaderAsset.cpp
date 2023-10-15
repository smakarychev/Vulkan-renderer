#include "ShaderAsset.h"

#include <nlm_json.hpp>
#include <stdbool.h>

#include "AssetLib.h"
#include "lz4.h"
#include "utils.h"

namespace
{
    using DescriptorFlags = assetLib::ShaderInfo::DescriptorSet::DescriptorFlags;

    DescriptorFlags flagsFromStringArray(const std::vector<std::string>& flagsStrings)
    {
        DescriptorFlags flags = DescriptorFlags::None;
        for (auto& flagString : flagsStrings)
        {
            if (flagString == assetLib::descriptorFlagToString(DescriptorFlags::Dynamic))
                flags |= DescriptorFlags::Dynamic;
            else if (flagString == assetLib::descriptorFlagToString(DescriptorFlags::Bindless))
                flags |= DescriptorFlags::Bindless;
            else
                ASSERT(false, "Unrecogrinzed flag {}", flagString)
        }
        
        return flags;
    }

    std::vector<std::string> stringArrayFromFlags(DescriptorFlags flags)
    {
        std::vector<std::string> flagsStrings;
        if (flags & DescriptorFlags::Dynamic)
            flagsStrings.push_back(assetLib::descriptorFlagToString(DescriptorFlags::Dynamic));
        if (flags & DescriptorFlags::Bindless)
            flagsStrings.push_back(assetLib::descriptorFlagToString(DescriptorFlags::Bindless));

        return flagsStrings;
    }
}

namespace assetLib
{
    ShaderInfo readShaderInfo(const assetLib::File& file)
    {
        ShaderInfo info = {};

        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        info.SourceSizeBytes = metadata["source_size_bytes"];
        info.ShaderStages = VkShaderStageFlags(metadata["shader_stage"].get<u32>());

        const nlohmann::json& inputAttributes = metadata["input_attributes"];
        u32 inputAttributeCount = (u32)inputAttributes.size();
        info.InputAttributes.reserve(inputAttributeCount);
        for (auto& input : inputAttributes)
        {
            ShaderInfo::InputAttribute inputAttribute = {};
            inputAttribute.Location = input["location"];
            inputAttribute.Name = input["name"];
            inputAttribute.Format = (VkFormat)input["format"];

            info.InputAttributes.push_back(inputAttribute);
        }

        const nlohmann::json& pushConstants = metadata["push_constants"];
        u32 pushConstantCount = (u32)pushConstants.size();
        info.PushConstants.reserve(pushConstantCount);
        for (auto& push : pushConstants)
        {
            ShaderInfo::PushConstant pushConstant = {};
            pushConstant.SizeBytes = push["size_bytes"];
            pushConstant.Offset = push["offset"];
            pushConstant.ShaderStages = (VkShaderStageFlags)push["shader_stages"];

            info.PushConstants.push_back(pushConstant);
        }

        const nlohmann::json& descriptorSets = metadata["descriptor_sets"];
        u32 descriptorSetCount = (u32)descriptorSets.size();
        info.DescriptorSets.reserve(descriptorSetCount);
        for (auto& set : descriptorSets)
        {
            ShaderInfo::DescriptorSet descriptorSet = {};
            descriptorSet.Set = set["set"];
            const nlohmann::json& bindings = set["bindings"];
            u32 bindingCount = (u32)bindings.size();
            descriptorSet.Descriptors.reserve(bindingCount);
            for (auto& binding : bindings)
            {
                ShaderInfo::DescriptorSet::DescriptorBinding descriptorBinding = {};
                descriptorBinding.Binding = binding["binding"];
                descriptorBinding.Name = binding["name"];
                descriptorBinding.Type = (VkDescriptorType)binding["descriptor"];
                descriptorBinding.ShaderStages = (VkShaderStageFlags)binding["shader_stages"];
                const nlohmann::json& flags = binding["flags"];
                std::vector<std::string> flagsStrings;
                flagsStrings.reserve(flags.size());
                for (auto& flag : flags)
                    flagsStrings.push_back(flag);
                descriptorBinding.Flags = flagsFromStringArray(flagsStrings);
                
                descriptorSet.Descriptors.push_back(descriptorBinding);
            }

            info.DescriptorSets.push_back(descriptorSet);
        }

        unpackAssetInfo(info, &metadata);
        
        return info;
    }

    assetLib::File packShader(const ShaderInfo& info, void* source)
    {
        nlohmann::json metadata;

        metadata["source_size_bytes"] = info.SourceSizeBytes;
        metadata["shader_stage"] = (u32)info.ShaderStages;
        
        metadata["input_attributes"] = nlohmann::json::array();
        for (auto& input : info.InputAttributes)
        {
            nlohmann::json inputJson;
            inputJson["location"] = input.Location;
            inputJson["name"] = input.Name;
            inputJson["format"] = (u32)input.Format;

            metadata["input_attributes"].push_back(inputJson);
        }

        metadata["push_constants"] = nlohmann::json::array();
        for (auto& push : info.PushConstants)
        {
            nlohmann::json pushJson;
            pushJson["size_bytes"] = push.SizeBytes;
            pushJson["offset"] = push.Offset;
            pushJson["shader_stages"] = (u32)push.ShaderStages;

            metadata["push_constants"].push_back(pushJson);
        }

        metadata["descriptor_sets"] = nlohmann::json::array();
        for (auto& set : info.DescriptorSets)
        {
            nlohmann::json setJson;
            setJson["set"] = set.Set;
            setJson["bindings"] = nlohmann::json::array();
            for (auto& binding : set.Descriptors)
            {
                nlohmann::json bindingJson;
                bindingJson["binding"] = binding.Binding;
                bindingJson["name"] = binding.Name;
                bindingJson["descriptor"] = (u32)binding.Type;
                bindingJson["shader_stages"] = (u32)binding.ShaderStages;
                bindingJson["flags"] = nlohmann::json::array();
                for (auto& flag : stringArrayFromFlags(binding.Flags))
                    bindingJson["flags"].push_back(flag);
                
                setJson["bindings"].push_back(bindingJson);
            }

            metadata["descriptor_sets"].push_back(setJson);
        }

        packAssetInfo(info, &metadata);
        
        assetLib::File assetFile = {};
        
        u64 blobSizeBytes = utils::compressToBlob(assetFile.Blob, source, info.SourceSizeBytes);
        metadata["asset"]["blob_size_bytes"] = blobSizeBytes;
        metadata["asset"]["type"] = assetTypeToString(AssetType::Shader);
        
        assetFile.JSON = metadata.dump(JSON_INDENT);

        return assetFile;
    }

    void unpackShader(ShaderInfo& info, const u8* source, u64 sourceSizeBytes, u8* spirv)
    {
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != info.SourceSizeBytes)
            LZ4_decompress_safe((const char*)source, (char*)spirv, (i32)sourceSizeBytes, (i32)info.SourceSizeBytes);
        else
            memcpy(spirv, source, info.SourceSizeBytes);
    }

    std::string descriptorFlagToString(ShaderInfo::DescriptorSet::DescriptorFlags flag)
    {
        if (flag == DescriptorFlags::None)
            return "none";
        if (flag == DescriptorFlags::Dynamic)
            return "dynamic";
        if (flag == DescriptorFlags::Bindless)
            return "bindless";
        ASSERT(false, "Unsupported flag")
        std::unreachable();
    }
}

