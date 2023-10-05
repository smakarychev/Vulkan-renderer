#include "ShaderAsset.h"

#include <nlm_json.hpp>

#include "AssetLib.h"
#include "lz4.h"
#include "utils.h"

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
            descriptorSet.Bindings.reserve(bindingCount);
            for (auto& binding : bindings)
            {
                ShaderInfo::DescriptorSet::DescriptorBinding descriptorBinding = {};
                descriptorBinding.Binding = binding["binding"];
                descriptorBinding.Name = binding["name"];
                descriptorBinding.Descriptor = (VkDescriptorType)binding["descriptor"];
                descriptorBinding.ShaderStages = (VkShaderStageFlags)binding["shader_stages"];

                descriptorSet.Bindings.push_back(descriptorBinding);
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
            for (auto& binding : set.Bindings)
            {
                nlohmann::json bindingJson;
                bindingJson["binding"] = binding.Binding;
                bindingJson["name"] = binding.Name;
                bindingJson["descriptor"] = (u32)binding.Descriptor;
                bindingJson["shader_stages"] = (u32)binding.ShaderStages;

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
}

