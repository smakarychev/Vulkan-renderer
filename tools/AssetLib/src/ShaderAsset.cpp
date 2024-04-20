#include "ShaderAsset.h"

#include <nlm_json.hpp>

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

            else if (flagString == assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSampler))
                flags |= DescriptorFlags::ImmutableSampler;

            else if (flagString == assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearest))
                flags |= DescriptorFlags::ImmutableSamplerNearest;

            else if (flagString == assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampEdge))
                flags |= DescriptorFlags::ImmutableSamplerClampEdge;

            else if (flagString == assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampEdge))
                    flags |= DescriptorFlags::ImmutableSamplerNearestClampEdge;

            else if (flagString == assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerClampBlack))
                    flags |= DescriptorFlags::ImmutableSamplerClampBlack;

            else if (flagString == assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampBlack))
                    flags |= DescriptorFlags::ImmutableSamplerNearestClampBlack;

            else if (flagString == assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerClampWhite))
                    flags |= DescriptorFlags::ImmutableSamplerClampWhite;

            else if (flagString == assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampWhite))
                    flags |= DescriptorFlags::ImmutableSamplerNearestClampWhite;

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

        if (flags & DescriptorFlags::ImmutableSampler)
            flagsStrings.push_back(assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSampler));
        
        if (flags & DescriptorFlags::ImmutableSamplerNearest)
            flagsStrings.push_back(assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearest));
        
        if (flags & DescriptorFlags::ImmutableSamplerClampEdge)
            flagsStrings.push_back(assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampEdge));

        if (flags & DescriptorFlags::ImmutableSamplerNearestClampEdge)
            flagsStrings.push_back(assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampEdge));
        
        if (flags & DescriptorFlags::ImmutableSamplerClampBlack)
            flagsStrings.push_back(assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerClampBlack));
        
        if (flags & DescriptorFlags::ImmutableSamplerNearestClampBlack)
            flagsStrings.push_back(assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampBlack));
        
        if (flags & DescriptorFlags::ImmutableSamplerClampWhite)
            flagsStrings.push_back(assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerClampWhite));
        
        if (flags & DescriptorFlags::ImmutableSamplerNearestClampWhite)
            flagsStrings.push_back(assetLib::descriptorFlagToString(
                DescriptorFlags::ImmutableSamplerNearestClampWhite));

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
        info.ShaderStages = metadata["shader_stage"];

        const nlohmann::json& includedFiles = metadata["included_files"];
        u32 includedFilesCount = (u32)includedFiles.size();
        info.IncludedFiles.reserve(includedFilesCount);
        for (auto& includedFile : includedFiles)
            info.IncludedFiles.push_back(includedFile);

        const nlohmann::json& specializationConstants = metadata["specialization_constants"];
        u32 constantsCount = (u32)specializationConstants.size();
        info.SpecializationConstants.reserve(constantsCount);
        for (auto& constant : specializationConstants)
        {
            ShaderInfo::SpecializationConstant specializationConstant = {};
            specializationConstant.Id = constant["id"];
            specializationConstant.Name = constant["name"];
            specializationConstant.ShaderStages = constant["shader_stages"];

            info.SpecializationConstants.push_back(specializationConstant);
        }
        
        const nlohmann::json& inputAttributes = metadata["input_attributes"];
        u32 inputAttributeCount = (u32)inputAttributes.size();
        info.InputAttributes.reserve(inputAttributeCount);
        for (auto& input : inputAttributes)
        {
            ShaderInfo::InputAttribute inputAttribute = {};
            inputAttribute.Binding = input["binding"];
            inputAttribute.Location = input["location"];
            inputAttribute.Name = input["name"];
            inputAttribute.Format = input["format"];
            inputAttribute.SizeBytes = input["size_bytes"];

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
            pushConstant.ShaderStages = push["shader_stages"];

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
                descriptorBinding.Type = binding["descriptor"];
                descriptorBinding.ShaderStages = binding["shader_stages"];
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

    assetLib::File packShader(const ShaderInfo& info, const void* source)
    {
        nlohmann::json metadata;

        metadata["source_size_bytes"] = info.SourceSizeBytes;
        metadata["shader_stage"] = (u32)info.ShaderStages;

        metadata["included_files"] = nlohmann::json::array();
        for (auto& includedFile : info.IncludedFiles)
            metadata["included_files"].push_back(includedFile);

        metadata["specialization_constants"] = nlohmann::json::array();
        for (auto& constant : info.SpecializationConstants)
        {
            nlohmann::json constantJson;
            constantJson["id"] = constant.Id;
            constantJson["name"] = constant.Name;
            constantJson["shader_stages"] = (u32)constant.ShaderStages;

            metadata["specialization_constants"].push_back(constantJson);
        }
        
        metadata["input_attributes"] = nlohmann::json::array();
        for (auto& input : info.InputAttributes)
        {
            nlohmann::json inputJson;
            inputJson["binding"] = input.Binding;
            inputJson["location"] = input.Location;
            inputJson["name"] = input.Name;
            inputJson["format"] = (u32)input.Format;
            inputJson["size_bytes"] = input.SizeBytes;

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
        switch (flag) {
        case DescriptorFlags::None:
            return "none";
        case DescriptorFlags::Dynamic:
            return "dynamic";
        case DescriptorFlags::Bindless:
            return "bindless";
        case DescriptorFlags::ImmutableSampler:
            return "immutable_sampler";
        case DescriptorFlags::ImmutableSamplerNearest:
            return "immutable_sampler_nearest";
        case DescriptorFlags::ImmutableSamplerClampEdge:
            return "immutable_sampler_clamp_edge";
        case DescriptorFlags::ImmutableSamplerNearestClampEdge:
            return "immutable_sampler_nearest_clamp_edge";
        case DescriptorFlags::ImmutableSamplerClampBlack:
            return "immutable_sampler_clamp_black";
        case DescriptorFlags::ImmutableSamplerNearestClampBlack:
            return "immutable_sampler_nearest_clamp_black";
        case DescriptorFlags::ImmutableSamplerClampWhite:
            return "immutable_sampler_clamp_white";
        case DescriptorFlags::ImmutableSamplerNearestClampWhite:
            return "immutable_sampler_nearest_clamp_white";
        default:
            ASSERT(false, "Unsupported flag")
        }
        std::unreachable(); 
    }
}

