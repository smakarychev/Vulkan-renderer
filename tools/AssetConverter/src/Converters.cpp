#include "Converters.h"

#include "AssetLib.h"
#include "TextureAsset.h"
#include "SceneAsset.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NOEXCEPTION 
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <mikktspace.h>

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/optimizer.hpp>

#include <vulkan/vulkan_core.h>
#include <spirv_reflect.h>

#include "utils.h"
#include "core.h"

// SPIV-reflect implementation
#include <spirv_reflect.cpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stack>
#include <execution>

namespace
{
    struct AssetPaths
    {
        std::filesystem::path AssetPath;
        std::filesystem::path BlobPath;
    };
    template <typename Fn>
    AssetPaths getAssetsPath(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& rawFilePath, Fn&& transform)
    {
        std::filesystem::path processedPath = rawFilePath.filename();

        std::filesystem::path currentPath = rawFilePath.parent_path();
        while (!std::filesystem::equivalent(currentPath, initialDirectoryPath))
        {
            if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
                break;
            processedPath = currentPath.filename() / processedPath;
            currentPath = currentPath.parent_path();
        }

        if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
            processedPath = currentPath.parent_path() / PROCESSED_ASSETS_DIRECTORY_NAME / processedPath;
        else
            processedPath = rawFilePath;

        AssetPaths paths = transform(processedPath);

        return paths;
    }
    
    template <typename Fn>
    bool needsConversion(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& path, Fn&& transform)
    {
        std::filesystem::path convertedPath = getAssetsPath(initialDirectoryPath, path,
            [&transform](std::filesystem::path& unprocessedPath)
            {
                AssetPaths paths;
                paths.AssetPath = unprocessedPath;
                transform(paths.AssetPath);
                return paths;
            }).AssetPath;

        if (!std::filesystem::exists(convertedPath))
            return true;

        auto originalTime = std::filesystem::last_write_time(path);
        auto convertedTime = std::filesystem::last_write_time(convertedPath);

        if (convertedTime < originalTime)
            return true;

        return false;
    }
}

bool TextureConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    return needsConversion(initialDirectoryPath, path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(TextureConverter::POST_CONVERT_EXTENSION);
    });
}

void TextureConverter::Convert(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    Convert(initialDirectoryPath, path, assetLib::TextureFormat::SRGBA8);
}

void TextureConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path,
    assetLib::TextureFormat format)
{
    std::cout << std::format("Converting texture file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });

    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8* pixels{nullptr};
    u64 sizeBytes{0};
    if (path.extension() == ".hdr")
    {
        pixels = (u8*)stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);    
        format = assetLib::TextureFormat::RGBA32;
        sizeBytes = 16llu * width * height; 
    }
    else
    {
        pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        sizeBytes = 4llu * width * height; 
    }

    assetLib::TextureInfo textureInfo = {};
    textureInfo.Format = format;
    textureInfo.Dimensions = {.Width = (u32)width, .Height = (u32)height, .Depth = 1};
    textureInfo.SizeBytes = sizeBytes; 
    textureInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    textureInfo.OriginalFile = std::filesystem::weakly_canonical(path).generic_string();
    textureInfo.BlobFile = std::filesystem::weakly_canonical(blobPath).generic_string();
    
    assetLib::File textureFile = assetLib::packTexture(textureInfo, pixels);
        
    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), textureFile);

    stbi_image_free(pixels);

    std::cout << std::format("Texture file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}

bool ShaderStageConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path, const Options& options)
{
    std::filesystem::path convertedPath = {};
    
    bool requiresConversion = needsConversion(initialDirectoryPath, path, [&](std::filesystem::path& converted)
    {
        converted.replace_filename(GetBakedFileName(converted, options));
        converted.replace_extension(ShaderStageConverter::POST_CONVERT_EXTENSION);
        convertedPath = converted;
    });

    if (requiresConversion)
        return true;

    assetLib::File shaderFile;
    assetLib::loadAssetFile(convertedPath.string(), shaderFile);
    assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);

    for (auto& includedFile : shaderInfo.IncludedFiles)
    {
        auto originalTime = std::filesystem::last_write_time(includedFile);
        auto convertedTime = std::filesystem::last_write_time(convertedPath);

        if (convertedTime < originalTime)
            return true;
    }

    return false;
}

void ShaderStageConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path)
{
    Bake(initialDirectoryPath, path);
}

std::optional<assetLib::ShaderStageInfo> ShaderStageConverter::Bake(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path, const Options& options)
{
    std::cout << std::format("Converting shader stage file {}\n", path.string());

    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [&options](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_filename(GetBakedFileName(processedPath, options));
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath.replace_filename(GetBakedFileName(processedPath, options));
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });
    
    shaderc_shader_kind shaderKind = shaderc_glsl_infer_from_source;
    if (path.extension().string() == ".vert")
        shaderKind = shaderc_vertex_shader;
    else if (path.extension().string() == ".frag")
        shaderKind = shaderc_fragment_shader;
    else if (path.extension().string() == ".comp")
        shaderKind = shaderc_compute_shader;

    std::ifstream file(path.string(), std::ios::in | std::ios::binary);
    std::string shaderSource((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<DescriptorFlagInfo> descriptorFlags = ReadDescriptorsFlags(shaderSource);
    std::vector<InputAttributeBindingInfo> inputBindingsInfo = ReadInputBindings(shaderSource);
    RemoveMetaKeywords(shaderSource);

    class FileIncluder : public shaderc::CompileOptions::IncluderInterface
    {
        struct FileInfo
        {
            std::string FileName;
            std::string FileContent;
        };
    public:
        FileIncluder (std::vector<std::string>* includedFiles)
            : IncluderInterface(), IncludedFiles(includedFiles) {}
        
        shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type,
            const char* requestingSource, size_t includeDepth) override
        {
            if (type != shaderc_include_type_relative)
                return new shaderc_include_result{"", 0,
                    "only relative #include (with '...') is supported",
                    std::string{"only relative #include (with '...') is supported"}.length(), nullptr};

            std::filesystem::path requestedPath = std::filesystem::path{requestingSource}.parent_path() /
                std::filesystem::path{requestedSource};

            std::string fileName = std::filesystem::weakly_canonical(requestedPath).generic_string();
            std::ifstream fileIn(fileName.data(), std::ios::ate | std::ios::binary);
            if (!fileIn.is_open())
                return new shaderc_include_result{"", 0,
                    "#include file not found",
                    std::string{"#include file not found"}.length(), nullptr};
            isize fileSizeBytes = fileIn.tellg();
            fileIn.seekg(0);
            std::string fileContent;
            fileContent.resize(fileSizeBytes);
            fileIn.read(fileContent.data(), fileSizeBytes);

            IncludedFiles->push_back(fileName);
            
            FileInfo* fileInfo = new FileInfo{
                .FileName = fileName,
                .FileContent = fileContent};

            return new shaderc_include_result{
                fileInfo->FileName.data(), fileInfo->FileName.length(),
                fileInfo->FileContent.data(), fileInfo->FileContent.length(),
                fileInfo};
        }
        void ReleaseInclude(shaderc_include_result* data) override
        {
            FileInfo* fileInfo = (FileInfo*)(data->user_data);
            delete fileInfo;
            delete data;
        }

        std::vector<std::string>* IncludedFiles;
    };
    
    shaderc::Compiler compiler;
    shaderc::CompileOptions shadercOptions;
    std::vector<std::string> includedFiles;
    shadercOptions.SetIncluder(std::make_unique<FileIncluder>(&includedFiles));
    shadercOptions.SetTargetSpirv(shaderc_spirv_version_1_6);
    shadercOptions.SetOptimizationLevel(shaderc_optimization_level_zero);
    for (auto&& [define, value] : options.Defines)
    {
        if (value.empty())
            shadercOptions.AddMacroDefinition(define);
        else
            shadercOptions.AddMacroDefinition(define, value);
    }
    
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderKind,
        path.string().c_str(), shadercOptions);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cout << std::format("Shader stage compilation error:\n {}", module.GetErrorMessage());
        return {};
    }
    
    std::vector<u32> spirv = {module.cbegin(), module.cend()};

    // produce reflection on unoptimized code
    assetLib::ShaderStageInfo shaderInfo = Reflect(spirv, descriptorFlags, inputBindingsInfo);
    shaderInfo.IncludedFiles = includedFiles;
    shaderInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    shaderInfo.OriginalFile = std::filesystem::weakly_canonical(path).generic_string();
    shaderInfo.BlobFile = std::filesystem::weakly_canonical(blobPath).generic_string();

    std::vector<u32> spirvOptimized;
    spirvOptimized.reserve(spirv.size());
    spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_4);
    optimizer.RegisterPerformancePasses(true);

    if (optimizer.Run(spirv.data(), spirv.size(), &spirvOptimized))
        spirv = spirvOptimized;

    shaderInfo.SourceSizeBytes = spirv.size() * sizeof(u32);

    assetLib::File shaderFile = assetLib::packShaderStage(shaderInfo, spirv.data());

    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), shaderFile);

    std::cout << std::format("Shader stage file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());

    return shaderInfo;
}

std::string ShaderStageConverter::GetBakedFileName(const std::filesystem::path& path, const Options& options)
{
    return std::format(
            "{}-{}{}",
            path.stem().string(),
            path.extension().string().substr(1),
            options.DefinesHash == 0 ? std::string{} : "-" + std::to_string(options.DefinesHash));
}

std::vector<ShaderStageConverter::DescriptorFlagInfo> ShaderStageConverter::ReadDescriptorsFlags(std::string_view shaderSource)
{
    // weird flex but ok
    static const std::vector FLAGS = {
        DescriptorFlags::Dynamic,
        DescriptorFlags::Bindless,
        DescriptorFlags::ImmutableSampler,
        DescriptorFlags::ImmutableSamplerNearest,
        DescriptorFlags::ImmutableSamplerClampEdge,
        DescriptorFlags::ImmutableSamplerNearestClampEdge,
        DescriptorFlags::ImmutableSamplerClampBlack,
        DescriptorFlags::ImmutableSamplerNearestClampBlack,
        DescriptorFlags::ImmutableSamplerClampWhite,
        DescriptorFlags::ImmutableSamplerNearestClampWhite,
        DescriptorFlags::ImmutableSamplerShadow,
        DescriptorFlags::ImmutableSamplerShadowNearest,
        DescriptorFlags::ImmutableSamplerReductionMin,
        DescriptorFlags::ImmutableSamplerReductionMax,
    };

    std::vector<DescriptorFlagInfo> descriptorFlagsUnmerged = {};
    
    for (auto flag : FLAGS)
    {
        std::string keyword = std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(flag);
            
        usize offset = 0;
        usize pos = shaderSource.find(keyword, offset);
        while (pos != std::string::npos)
        {
            // find descriptor's name
            offset = pos + keyword.length();
            usize curlyPos, semicolonPos;
            curlyPos = shaderSource.find("{", offset);
            semicolonPos = shaderSource.find(";", offset);
            if (semicolonPos == std::string::npos)
                return {}; // error will be reported by shader compiler
            usize endingSemicolon = semicolonPos;
            if (curlyPos != std::string::npos && curlyPos < semicolonPos)
            {
                curlyPos = shaderSource.find("}", curlyPos);
                endingSemicolon = shaderSource.find(";", curlyPos);
            }

            usize textEnd = shaderSource.find_last_not_of(" \t\r\n", endingSemicolon);
            usize textStart = shaderSource.find_last_of(" \t\r\n", textEnd) + 1;

            std::string name = std::string{shaderSource.substr(textStart, textEnd - textStart)};
            if (name.ends_with("[]"))
                name = name.substr(0, name.size() - std::strlen("[]"));
            descriptorFlagsUnmerged.push_back({.Flags = flag, .DescriptorName = name});

            offset = textEnd;
            pos = shaderSource.find(keyword, offset);
        }
    }

    if (descriptorFlagsUnmerged.size() <= 1)
        return descriptorFlagsUnmerged;
    
    std::ranges::sort(descriptorFlagsUnmerged,
        [](const std::string& a, const std::string& b)
        {
            return a.compare(b);
        },
        [](const auto& a)
        {
            return a.DescriptorName;
        });
    
    std::vector<DescriptorFlagInfo> descriptorFlags;
    descriptorFlags.emplace_back(descriptorFlagsUnmerged.front());
    for (u32 i = 1; i < descriptorFlagsUnmerged.size(); i++)
    {
        if (descriptorFlags.back().DescriptorName == descriptorFlagsUnmerged[i].DescriptorName)
            descriptorFlags.back().Flags |= descriptorFlagsUnmerged[i].Flags;
        else
            descriptorFlags.emplace_back(descriptorFlagsUnmerged[i]);
    }
    
    return descriptorFlags;
}

std::vector<ShaderStageConverter::InputAttributeBindingInfo> ShaderStageConverter::ReadInputBindings(
    std::string_view shaderSource)
{
    struct AttributeInfo
    {
        usize TextPosition;
        std::string Name;
    };

    struct BindingInfo
    {
        usize TextPosition;
        u32 Binding;
    };
    
    auto findAttributes = [](std::string_view source) -> std::vector<AttributeInfo>
    {
        std::vector<AttributeInfo> attributes;

        static std::string keyword = "layout";
                
        usize offset = 0;
        usize startPos = source.find(keyword, offset);
        while (startPos != std::string::npos)
        {
            offset = startPos + keyword.length();
            usize endingSemicolon = source.find(";", offset);
            if (endingSemicolon == std::string::npos)
                return {}; // error will be reported by shader compiler
            usize inputMark = source.find(" in ", offset);
            if (inputMark != std::string::npos && inputMark < endingSemicolon)
            {
                // the attribute is input attribute
                usize textEnd = source.find_last_not_of(" \t\r\n", endingSemicolon);
                usize textStart = source.find_last_of(" \t\r\n", textEnd) + 1;

                attributes.push_back({
                    .TextPosition = textStart,
                    .Name = std::string{source.substr(textStart, textEnd - textStart)}});
            }

            offset = endingSemicolon;
            startPos = source.find(keyword, offset);
        }

        return attributes;
    };

    auto findBindings = [](std::string_view source) -> std::vector<BindingInfo>
    {
        std::vector<BindingInfo> bindings;

        static std::string bindingKeyword = "binding";
        std::string keyword = std::string{META_KEYWORD_PREFIX} + bindingKeyword;

        usize offset = 0;
        usize startPos = source.find(keyword, offset);
        while (startPos != std::string::npos)
        {
            offset = startPos + keyword.length();
            usize bindingNumberStringEnd = std::min(source.find("\r\n", offset), source.find("\n", offset));
            usize bindingNumberStringStart = source.find_last_of(" \t", bindingNumberStringEnd) + 1;

            u32 binding = std::stoul(std::string{source.substr(bindingNumberStringStart,
                bindingNumberStringEnd - bindingNumberStringStart)});
            bindings.push_back({.TextPosition = bindingNumberStringStart, .Binding = binding});

            offset = bindingNumberStringEnd;
            startPos = source.find(keyword, offset);
        }

        return bindings;
    };

    std::vector<AttributeInfo> attributes = findAttributes(shaderSource);
    std::vector<BindingInfo> bindings = findBindings(shaderSource);

    if (attributes.empty())
        return {};

    if (bindings.empty())
    {
        std::vector<InputAttributeBindingInfo> inputAttributeBindingInfos;
        inputAttributeBindingInfos.reserve(attributes.size());
        for (auto& attribute : attributes)
            inputAttributeBindingInfos.push_back({.Binding = 0, .Attribute = attribute.Name});

        return inputAttributeBindingInfos;
    }

    if (attributes.front().TextPosition < bindings.front().TextPosition)
    {
        auto it = std::ranges::find_if(bindings,
            [](auto& bindingInfo){ return bindingInfo.Binding == 0;});
        if (it != bindings.end())
            LOG("WARNING: dangerous implicit binding 0 for some input attributes");

        bindings.insert(bindings.begin(), {.TextPosition = 0, .Binding = 0});
    }

    std::vector<InputAttributeBindingInfo> inputAttributeBindingInfos;
    inputAttributeBindingInfos.reserve(attributes.size());
    
    u32 bindingIndexCurrent = 0;
    u32 bindingIndexNext = bindingIndexCurrent + 1;

    u32 attributeIndexCurrent = 0;

    while (attributeIndexCurrent < attributes.size())
    {
        AttributeInfo& attribute = attributes[attributeIndexCurrent];
        if (bindingIndexNext == bindings.size() || attribute.TextPosition < bindings[bindingIndexNext].TextPosition)
        {
            inputAttributeBindingInfos.push_back({
                .Binding = bindings[bindingIndexCurrent].Binding,
                .Attribute = attribute.Name});
            attributeIndexCurrent++;
        }
        else
        {
            bindingIndexCurrent = bindingIndexNext;
            bindingIndexNext++;
        }
    }

    return inputAttributeBindingInfos;
}

void ShaderStageConverter::RemoveMetaKeywords(std::string& shaderSource)
{
    static const std::vector keywords = {
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::Dynamic),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::Bindless),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSampler),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearest),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampEdge),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampEdge),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampBlack),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampBlack),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampWhite),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampWhite),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerShadow),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerShadowNearest),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerReductionMin),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerReductionMax),
        std::string{META_KEYWORD_PREFIX} + "binding",
    };

    for (auto keyword : keywords)
    {
        usize pos = shaderSource.find(keyword, 0);
        while (pos != std::string::npos)
        {
            shaderSource.replace(pos, 2, "//");
            pos = shaderSource.find(keyword, pos);
        }
    }
}

assetLib::ShaderStageInfo ShaderStageConverter::Reflect(const std::vector<u32>& spirV,
        const std::vector<DescriptorFlagInfo>& flags, const std::vector<InputAttributeBindingInfo>& inputBindings)
{
    static constexpr u32 SPV_INVALID_VAL = (u32)~0;
    
    assetLib::ShaderStageInfo shaderInfo = {};
    
    SpvReflectShaderModule reflectedModule = {};
    spvReflectCreateShaderModule(spirV.size() * sizeof(u32), spirV.data(), &reflectedModule);

    shaderInfo.ShaderStages = (u32)reflectedModule.shader_stage;

    // extract specialization constants
    u32 specializationCount;
    spvReflectEnumerateSpecializationConstants(&reflectedModule, &specializationCount, nullptr);
    std::vector<SpvReflectSpecializationConstant*> specializationConstants(specializationCount);
    spvReflectEnumerateSpecializationConstants(&reflectedModule, &specializationCount, specializationConstants.data());

    shaderInfo.SpecializationConstants.reserve(specializationCount);
    for (auto& constant : specializationConstants)
        shaderInfo.SpecializationConstants.push_back({
            .Name = constant->name,
            .Id = constant->constant_id,
            .ShaderStages = (u32)reflectedModule.shader_stage});

    // extract input attributes
    if (reflectedModule.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    {
        u32 inputCount;
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, inputs.data());

        shaderInfo.InputAttributes.reserve(inputCount);
        for (auto& input : inputs)
        {
            if (input->location == SPV_INVALID_VAL)
                continue;
            shaderInfo.InputAttributes.push_back({
                .Name = input->name,
                .Location = input->location,
                .Format = (u32)input->format,
                .SizeBytes = input->numeric.scalar.width * std::max(1u, input->numeric.vector.component_count) / 8});
            for (auto& inputBinding : inputBindings)
                if (inputBinding.Attribute == input->name)
                {
                    shaderInfo.InputAttributes.back().Binding = inputBinding.Binding;
                    break;
                }
        }
        std::ranges::sort(shaderInfo.InputAttributes,
            [](const auto& a, const auto& b) { return a.Binding < b.Binding ||
                a.Binding == b.Binding && a.Location < b.Location; });
    }

    // extract push constants
    u32 pushCount;
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, nullptr);
    std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, pushConstants.data());

    shaderInfo.PushConstants.reserve(pushCount);
    for (auto& push : pushConstants)
        shaderInfo.PushConstants.push_back({
            .SizeBytes = push->size,
            .Offset = push->offset,
            .ShaderStages = (u32)reflectedModule.shader_stage});
    std::ranges::sort(shaderInfo.PushConstants,
        [](const auto& a, const auto& b) { return a.Offset < b.Offset; });

    // extract descriptors
    u32 setCount;
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(setCount);
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, sets.data());

    if (setCount > MAX_PIPELINE_DESCRIPTOR_SETS)
    {
        std::cout << std::format("Can have only {} different descriptor sets, but have {}",
            MAX_PIPELINE_DESCRIPTOR_SETS, setCount);
        return shaderInfo;
    }

    shaderInfo.DescriptorSets.reserve(setCount);
    for (auto& set : sets)
    {
        shaderInfo.DescriptorSets.push_back({.Set = set->set});
        
        assetLib::ShaderStageInfo::DescriptorSet& descriptorSet = shaderInfo.DescriptorSets.back();
        descriptorSet.Descriptors.resize(set->binding_count);
        for (u32 i = 0; i < set->binding_count; i++)
        {
            descriptorSet.Descriptors[i] = {
                .Name = set->bindings[i]->name,
                .Count = set->bindings[i]->count,
                .Binding = set->bindings[i]->binding,
                .Type = (u32)set->bindings[i]->descriptor_type,
                .ShaderStages = (u32)reflectedModule.shader_stage
            };
            auto it = std::ranges::find_if(flags,
               [&descriptorSet, i](const auto& flag)
               {
                   return flag.DescriptorName == descriptorSet.Descriptors[i].Name;
               });

            if (it != flags.end())
                descriptorSet.Descriptors[i].Flags = it->Flags;
            
            if (descriptorSet.Descriptors[i].Flags & DescriptorFlags::Dynamic)
            {
                if (descriptorSet.Descriptors[i].Type == (u32)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    descriptorSet.Descriptors[i].Type = (u32)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                else if (descriptorSet.Descriptors[i].Type == (u32)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    descriptorSet.Descriptors[i].Type = (u32)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }
        }
        std::ranges::sort(descriptorSet.Descriptors,
                          [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
    }
    std::ranges::sort(shaderInfo.DescriptorSets,
                      [](const auto& a, const auto& b) { return a.Set < b.Set; });

    return shaderInfo;
}




bool SceneConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    return needsConversion(initialDirectoryPath, path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(SceneConverter::POST_CONVERT_EXTENSION);
    });
}

namespace glm
{
    void to_json(nlohmann::json& j, const glm::vec4& vec)
    {
        j = { { "r", vec.x }, { "g", vec.y }, { "b", vec.z }, { "a", vec.w } };
    }
    
    void from_json(const nlohmann::json& j, glm::vec4& vec)
    {
        vec.x = j.at("r").get<f32>();
        vec.y = j.at("g").get<f32>();
        vec.z = j.at("b").get<f32>();
        vec.w = j.at("a").get<f32>();
    }

    void to_json(nlohmann::json& j, const glm::vec3& vec)
    {
        j = { { "x", vec.x }, { "y", vec.y }, { "z", vec.z } };
    }
    
    void from_json(const nlohmann::json& j, glm::vec3& vec)
    {
        vec.x = j.at("x").get<f32>();
        vec.y = j.at("y").get<f32>();
        vec.z = j.at("z").get<f32>();
    }
}

namespace
{
    struct WriteResult
    {
        u64 Offset{};
    };
    template <usize Alignment>
    WriteResult writeAligned(std::vector<u8>& destination, const void* data, u64 size)
    {
        static_assert((Alignment & (Alignment - 1)) == 0);

        const char zeros[Alignment]{};
        const u64 pos = destination.size();
        const u64 padding = (Alignment - (pos & (Alignment - 1))) & (Alignment - 1);
        destination.resize(destination.size() + padding + size);
        memcpy(destination.data() + pos, zeros, padding);
        memcpy(destination.data() + pos + padding, data, size);

        return {.Offset = pos + padding};
    }

    template <typename T>
    struct AccessorDataTypeTraits
    {
        static_assert(sizeof(T) == 0, "No match for type");
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec4>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC4;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec3>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC3;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec2>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC2;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<u8>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_SCALAR;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    };
    
    struct SceneProcessContext
    {
        tinygltf::Model BakedScene{};

        template <typename T>
        struct AccessorProxy
        {
            u32 ViewIndex{0};
            std::vector<T> Data;
        };
        AccessorProxy<glm::vec3> Positions;
        AccessorProxy<glm::vec3> Normals;
        AccessorProxy<glm::vec4> Tangents;
        AccessorProxy<glm::vec2> UVs;
        AccessorProxy<assetLib::SceneInfo::IndexType> Indices;
        AccessorProxy<assetLib::SceneInfo::Meshlet> Meshlets;
        
        std::string BinaryName{};
        std::filesystem::path BakedScenePath{};

        void Prepare()
        {
            namespace fs = std::filesystem;
            fs::create_directories(BakedScenePath.parent_path());
            if (fs::exists(BakedScenePath))
                fs::remove(BakedScenePath);
            
            BakedScene.accessors.clear();
            BakedScene.buffers.clear();
            BakedScene.bufferViews.clear();
            BakedScene.meshes.clear();

            BakedScene.buffers.resize(1);
            BakedScene.buffers[0].uri = BinaryName;

            BakedScene.bufferViews.resize((u32)assetLib::SceneInfo::BufferViewType::MaxVal);
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Position].name = "Positions";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Normal].name = "Normals";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Tangent].name = "Tangents";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Uv].name = "UVs";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Index].name = "Indices";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Meshlet].name = "Meshlets";
            for (auto& view : BakedScene.bufferViews)
                view.buffer = 0;

            Positions.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Position;
            Normals.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Normal;
            Tangents.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Tangent;
            UVs.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Uv;
            Indices.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Index;
            Meshlets.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Meshlet;
        }

        void Finalize()
        {
            static constexpr u32 ALIGNMENT = 4;

            auto writeAndUpdateView = [this](auto accessorProxy) {
                const u64 sizeBytes = accessorProxy.Data.size() * sizeof(accessorProxy.Data[0]);
                WriteResult write = writeAligned<ALIGNMENT>(BakedScene.buffers[0].data,
                    accessorProxy.Data.data(), sizeBytes);
                BakedScene.bufferViews[accessorProxy.ViewIndex].byteOffset = write.Offset;
                BakedScene.bufferViews[accessorProxy.ViewIndex].byteLength = sizeBytes;
            };
            
            writeAndUpdateView(Positions);
            writeAndUpdateView(Normals);
            writeAndUpdateView(Tangents);
            writeAndUpdateView(UVs);
            writeAndUpdateView(Indices);
            writeAndUpdateView(Meshlets);

            tinygltf::TinyGLTF writer = {};
            writer.SetImageWriter(nullptr, nullptr);
            writer.WriteGltfSceneToFile(&BakedScene, BakedScenePath.string());
        }

        template <typename T>
        tinygltf::Accessor CreateAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
        {
            tinygltf::Accessor accessor {};
            accessor.componentType = AccessorDataTypeTraits<T>::COMPONENT_TYPE;
            accessor.type = AccessorDataTypeTraits<T>::TYPE;
            accessor.count = data.size();
            accessor.bufferView = accessorProxy.ViewIndex;
            accessor.byteOffset = accessorProxy.Data.size() * sizeof(T);
            accessorProxy.Data.append_range(data);

            return accessor;
        }

        template <typename T>
        nlohmann::json CreatePseudoAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
        {
            nlohmann::json accessor = {};
            accessor["count"] = data.size();
            accessor["bufferView"] = accessorProxy.ViewIndex;
            accessor["byteOffset"] = accessorProxy.Data.size() * sizeof(T);
            accessorProxy.Data.append_range(data);

            return accessor;
        }
    };
    
    template <typename T>
    void copyBufferToVector(std::vector<T>& vec, tinygltf::Model& gltf, tinygltf::Accessor& accessor)
    {
        const tinygltf::BufferView& bufferView = gltf.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf.buffers[bufferView.buffer];
        const u64 elementSizeBytes =
            (u64)tinygltf::GetNumComponentsInType(accessor.type) *
            (u64)tinygltf::GetComponentSizeInBytes(accessor.componentType);
        ASSERT(elementSizeBytes == sizeof(T))
        const u64 sizeBytes = elementSizeBytes * accessor.count;
        const u64 offset = bufferView.byteOffset + accessor.byteOffset;
        const u32 stride = accessor.ByteStride(bufferView);

        vec.resize(accessor.count);

        if (stride == elementSizeBytes)
            memcpy(vec.data(), buffer.data.data() + offset, sizeBytes);
        else
            for (u32 i = 0; i < accessor.count; i++)
                memcpy(vec.data() + i * elementSizeBytes,
                    buffer.data.data() + offset + (u64)i * stride,
                    elementSizeBytes);
    }

    void generateTriangleNormals(std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& positions, const std::vector<u32> indices)
    {
        normals.resize(positions.size());
        for (u32 i = 0; i < indices.size(); i += 3)
        {
            const glm::vec3 a = positions[indices[i + 1]] - positions[indices[i + 0]];   
            const glm::vec3 b = positions[indices[i + 2]] - positions[indices[i + 0]];
            const glm::vec3 normal = glm::normalize(glm::cross(a, b));
            normals[indices[i + 0]] = normal;
            normals[indices[i + 1]] = normal;
            normals[indices[i + 2]] = normal;
        }
    }

    void generateTriangleTangents(std::vector<glm::vec4>& tangents,
        const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec2>& uvs, const std::vector<u32>& indices)
    {
        struct GeometryInfo
        {
            const std::vector<glm::vec3>* Positions{};
            const std::vector<glm::vec3>* Normals{};
            const std::vector<glm::vec2>* Uvs{};
            std::vector<glm::vec4>* Tangents{};
            const std::vector<u32>* Indices{};
        };
        GeometryInfo gi = {
            .Positions = &positions,
            .Normals = &normals,
            .Uvs = &uvs,
            .Tangents = &tangents,
            .Indices = &indices};

        tangents.resize(normals.size());
        
        SMikkTSpaceInterface interface = {};
        interface.m_getNumFaces = [](const SMikkTSpaceContext* ctx) {
            return (i32)((GeometryInfo*)ctx->m_pUserData)->Indices->size() / 3;  
        };
        interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const i32){ return 3; };
        interface.m_getPosition = [](const SMikkTSpaceContext* ctx, f32 position[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(position, &(*info->Positions)[index], sizeof(glm::vec3));
        };
        interface.m_getNormal = [](const SMikkTSpaceContext* ctx, f32 normal[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(normal, &(*info->Normals)[index], sizeof(glm::vec3));
        };
        interface.m_getTexCoord = [](const SMikkTSpaceContext* ctx, f32 uv[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(uv, &(*info->Uvs)[index], sizeof(glm::vec2));
        };
        interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx, const f32 tangent[], const f32 sign,
            const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            
            memcpy(&(*info->Tangents)[index], tangent, sizeof(glm::vec3));
            (*info->Tangents)[index][3] = sign;
        };

        SMikkTSpaceContext context = {};
        context.m_pUserData = &gi;
        context.m_pInterface = &interface;
        genTangSpaceDefault(&context);
    }
    
    void processMesh(SceneProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Mesh& mesh)
    {
        for (auto& primitive : mesh.primitives)
        {
            tinygltf::Mesh backedMesh = mesh;
            backedMesh.primitives = {};
            
            ASSERT(primitive.mode == TINYGLTF_MODE_TRIANGLES)
            
            tinygltf::Accessor& indexAccessor = gltf.accessors[primitive.indices];
            std::vector<u32> indices(indexAccessor.count);
            switch (indexAccessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    {
                        std::vector<u8> rawIndices(indexAccessor.count);
                        copyBufferToVector(rawIndices, gltf, indexAccessor);
                        indices.assign_range(rawIndices);
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        std::vector<u16> rawIndices(indexAccessor.count);
                        copyBufferToVector(rawIndices, gltf, indexAccessor);
                        indices.assign_range(rawIndices);
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    copyBufferToVector(indices, gltf, indexAccessor);
                    break;
                default:
                    ASSERT(false)
            }

            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec4> tangents;
            std::vector<glm::vec2> uvs;
            for (auto& attribute : primitive.attributes)
            {
                tinygltf::Accessor& attributeAccessor = gltf.accessors[attribute.second];
                if (attribute.first == "POSITION")
                    copyBufferToVector(positions, gltf, attributeAccessor);
                else if (attribute.first == "NORMAL")
                    copyBufferToVector(normals, gltf, attributeAccessor);
                else if (attribute.first == "TANGENT")
                    copyBufferToVector(tangents, gltf, attributeAccessor);
                else if (attribute.first == "TEXCOORD_0")
                    copyBufferToVector(uvs, gltf, attributeAccessor);
            }

            ASSERT(!positions.empty(), "The mesh has no positions")

            const bool hasNormals = !normals.empty();
            const bool hasTangents = !tangents.empty();
            const bool hasUVs = !uvs.empty();
            
            if (!hasNormals)
                generateTriangleNormals(normals, positions, indices);
            if (!hasTangents && hasUVs)
                generateTriangleTangents(tangents, positions, normals, uvs, indices);
            if (!hasTangents)
                tangents.resize(positions.size(), glm::vec4{0.0f, 0.0f, 1.0f, 1.0f});
            if (!hasUVs)
                uvs.resize(positions.size(), glm::vec2{0.0});

            utils::Attributes attributes {
                .Positions = &positions,
                .Normals = &normals,
                .Tangents = &tangents,
                .UVs = &uvs};
            utils::remapMesh(attributes, indices);
            auto&& [meshlets, meshletIndices] = utils::createMeshlets(attributes, indices);
            auto&& [sphere, box] = utils::meshBoundingVolumes(meshlets);

            i32 currentAccessorIndex = (i32)ctx.BakedScene.accessors.size();
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(positions, ctx.Positions));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(normals, ctx.Normals));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(tangents, ctx.Tangents));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(uvs, ctx.UVs));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(meshletIndices, ctx.Indices));           
            nlohmann::json meshletsAccessor = ctx.CreatePseudoAccessor(meshlets, ctx.Meshlets);

            tinygltf::Primitive backedPrimitive = primitive;
            nlohmann::json extras = {};
            extras["bounding_box"]["min"] = box.Min;
            extras["bounding_box"]["max"] = box.Max;
            extras["bounding_sphere"]["center"] = sphere.Center;
            extras["bounding_sphere"]["radius"] = sphere.Radius;
            extras["meshlets"] = meshletsAccessor;
            tinygltf::ParseJsonAsValue(&backedPrimitive.extras, extras);

            backedPrimitive.attributes = {
                {"POSITION",    currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Position},
                {"NORMAL",      currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Normal},
                {"TANGENT",     currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Tangent},
                {"TEXCOORD_0",  currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Uv},
            };
            backedPrimitive.indices = (i32)currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Index;
            
            backedMesh.primitives.push_back(backedPrimitive);
            ctx.BakedScene.meshes.push_back(backedMesh);
        }
    }
}

void SceneConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path)
{
    LOG("Converting scene file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath = paths.BlobPath.filename();
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });
    
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    loader.ShouldPreloadBuffersData(false);
    loader.ShouldPreloadImagesData(false);
    std::string errors;
    std::string warnings;
    bool success = loader.LoadASCIIFromFile(&gltf, &errors, &warnings, path.string());
    success = success && loader.LoadBuffersData(&gltf, &errors, &warnings, path.parent_path().string());
    success = success && loader.LoadImagesData(&gltf, &errors, &warnings, path.parent_path().string());
    
    if (!errors.empty())
        LOG("ERROR: {} ({})", errors, path.string());
    if (!warnings.empty())
        LOG("WARNING: {} ({})", warnings, path.string());
    if (!success)
        return;
    if (gltf.scenes.empty())
        return;

    /* process just one scene for now */
    const u32 sceneIndex = gltf.defaultScene > 0 ? (u32)gltf.defaultScene : 0;
    auto& scene = gltf.scenes[sceneIndex];
    if (scene.nodes.empty())
        return;

    SceneProcessContext ctx = {};
    ctx.BinaryName = blobPath.string();
    ctx.BakedScenePath = assetPath;
    ctx.BakedScene = gltf;
    ctx.Prepare();
    
    for (auto& mesh : gltf.meshes)
        processMesh(ctx, gltf, mesh);

    ctx.Finalize();

    LOG("Scene file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}
