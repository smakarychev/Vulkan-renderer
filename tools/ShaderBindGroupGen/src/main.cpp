#include "types.h"
#include "core.h"

#include "ShaderAsset.h"

#include <inja/inja.hpp>
#include <fstream>
#include <spirv_reflect.h>
#include <unordered_set>

#include "Platform/PlatformUtils.h"
#include "Utils/HashUtils.h"

namespace fs = std::filesystem;

static const fs::path TEMPLATE_PATH = "./templates/";
static constexpr std::string_view BIND_GROUP_TEMPLATE_BASE = "bindgroups.tmpl";
static constexpr std::string_view BIND_GROUP_TEMPLATE_SPEC = "bindgroup-spec.tmpl";
static constexpr std::string_view BIND_GROUP_GENERATED_BASE_NAME = "ShaderBindGroupBase.generated.h";
static constexpr std::string_view SHADER_EXTENSION = ".shader";

/* Make canonical version of name
 * some_interesting-name -> SomeInterestingName
 */
std::string canonicalizeName(std::string_view name)
{
    if (name.empty())
        return std::string{name};

    std::string canonicalized;
    canonicalized.reserve(name.size());
    bool shouldCapitalize = true;
    for (char c : name)
    {
        switch (c)
        {
        case '-':
        case '_':
            shouldCapitalize = true;
            break;
        default:
            canonicalized.push_back(shouldCapitalize ? (char)toupper(c) : c);
            shouldCapitalize = false;
            break;
        }
    }

    return canonicalized;
}

/* Make canonical version of binding name
 * u_some_interesting-name -> SomeInterestingName
 */
std::string canonicalizeBindingName(std::string_view name)
{
    static constexpr std::string_view BINDING_PREFIX = "u_";
    static constexpr std::string_view UGB_BINDING_PREFIX = "u_ugb_";
    static constexpr std::string_view UGB_CANONICAL_NAME = "UGB";

    if (name.starts_with(UGB_BINDING_PREFIX))
        return std::string{UGB_CANONICAL_NAME};

    if (!name.starts_with(BINDING_PREFIX))
    {
        LOG("Warning: binding name does not start with {}", BINDING_PREFIX);
        return canonicalizeName(name);
    }

    return canonicalizeName(name.substr(BINDING_PREFIX.size()));
}

nlohmann::json loadShaderInfos(const fs::path& shadersPath)
{
    nlohmann::json templateData = {};
    for (const auto& file : fs::recursive_directory_iterator(shadersPath))
    {
        if (file.is_directory())
            continue;
        if (file.path().extension() != SHADER_EXTENSION)
            continue;

        std::ifstream in(file.path());
        nlohmann::json shader = nlohmann::json::parse(in);

        std::unordered_set<std::string> processedBindings;
        nlohmann::json shaderTemplateData = {};
        shaderTemplateData["name"] = canonicalizeName(shader["name"]);
        shaderTemplateData["is_raster"] = true;
        shaderTemplateData["has_samplers"] = false;
        shaderTemplateData["has_resources"] = false;
        shaderTemplateData["has_materials"] = false;
        shaderTemplateData["has_immutable_sampler"] = false;
        
        auto& stages = shader["shader_stages"];
        for (const auto& stage : stages)
        {
            assetLib::File shaderFile;
            assetLib::loadAssetFile(fs::weakly_canonical(shadersPath / std::string{stage}).string(), shaderFile);

            nlohmann::json stageJson = nlohmann::json::parse(shaderFile.JSON);
            for (const auto& set : stageJson["descriptor_sets"])
            {
                u32 setIndex = set["set"];
                if (setIndex == 0)
                    shaderTemplateData["has_samplers"] = true;
                else if (setIndex == 1)
                    shaderTemplateData["has_resources"] = true;
                else if (setIndex == 2)
                    shaderTemplateData["has_materials"] = true;
                
                for (const auto& binding : set["bindings"])
                {
                    const std::string name = canonicalizeBindingName(binding["name"]);
                    if (processedBindings.contains(name))
                        continue;

                    processedBindings.emplace(name);

                    nlohmann::json bindingJson = {};
                    bindingJson["name"] = name;
                    bindingJson["set"] = setIndex;
                    bindingJson["binding"] = (u32)binding["binding"];
                    bindingJson["count"] = (u32)binding["count"];
                    bindingJson["descriptor"] = (u32)binding["descriptor"];
                    bindingJson["is_bindless"] = false;
                    bool isImmutableSampler = false;
                    for (auto& flag : binding["flags"])
                    {
                        if (flag == "bindless")
                            bindingJson["is_bindless"] = true;
                        else if (flag == "immutable_sampler")
                            isImmutableSampler = true;
                    }
                    /* do not add Set method for immutable sampler */
                    if (isImmutableSampler)
                        shaderTemplateData["has_immutable_sampler"] = true;
                    else
                        shaderTemplateData["bindings"].emplace_back(bindingJson);

                    if ((SpvReflectShaderStageFlagBits)binding["shader_stages"] & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
                        shaderTemplateData["is_raster"] = false;
                }
            }
        }

        templateData["shaders"].emplace_back(shaderTemplateData);
    }

    return templateData;
}

bool shouldUpdateFile(const fs::path& path, std::string_view newContent)
{
    if (!fs::exists(path))
        return true;
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG("Failed to open file {}", path.string());
        return false;
    }

    const isize filesSize = file.tellg();
    if (filesSize != (isize)newContent.size())
        return true;
    
    file.seekg(0);
    std::vector<u8> buffer(filesSize);
    file.read(reinterpret_cast<char*>(buffer.data()), filesSize);

    return
        Hash::murmur3b32(buffer.data(), filesSize) != Hash::murmur3b32((const u8*)newContent.data(), newContent.size());
}

void updateFileIfNeeded(const fs::path& outputPath, std::string_view newContent)
{
    if (!shouldUpdateFile(outputPath, newContent))
    {
        LOG("Skipping {}: no changes detected", outputPath.string()); 
        return;
    }

    LOG("Creating file {}", outputPath.string());
    std::ofstream out(outputPath, std::ios::binary);
    out.write(newContent.data(), (isize)newContent.size());
}

void generateTemplates(inja::Environment& env, const fs::path& outputPath, nlohmann::json& data)
{
    {
        data["base_template_name"] = BIND_GROUP_GENERATED_BASE_NAME;
        inja::Template baseTemplate = env.parse_template((TEMPLATE_PATH / BIND_GROUP_TEMPLATE_BASE).string());
        std::string baseTemplateRendered = env.render(baseTemplate, data);
        updateFileIfNeeded(outputPath / BIND_GROUP_GENERATED_BASE_NAME, baseTemplateRendered);
    }

    for (auto& shader : data["shaders"])
    {
        shader["base_template_name"] = BIND_GROUP_GENERATED_BASE_NAME;
        inja::Template shaderTemplate;
        try
        {
            shaderTemplate = env.parse_template((TEMPLATE_PATH / BIND_GROUP_TEMPLATE_SPEC).string());
        }
        catch (std::exception& e)
        {
            LOG("Template parse exception: {}", e.what());
        }
        std::string shaderTemplateRendered = env.render(shaderTemplate, shader);
        updateFileIfNeeded(outputPath / (std::string{shader["name"]} + "BindGroup.generated.h"),
            shaderTemplateRendered);
    }
}

i32 main(i32 argc, char** argv)
{
    if (argc < 3)
    {
        LOG("Usage: ShaderBindGroupGen <shaders_directory> <output_directory>");
        return 1;
    }
    fs::current_path(Platform::getExecutablePath().parent_path());
    
    fs::path shadersPath = std::filesystem::weakly_canonical(argv[1]);
    fs::path outputPath = std::filesystem::weakly_canonical(argv[2]);
    LOG("Shaders path: {}", shadersPath.string());
    LOG("Output path: {}", outputPath.string());
    if (!fs::exists(outputPath))
        fs::create_directories(outputPath);
        
    loadShaderInfos(shadersPath);

    inja::Environment env = {};
    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);
    env.add_callback("is_descriptor_a_buffer", 1, [](inja::Arguments& args) {
        const u32 descriptorType = args.at(0)->get<u32>();
        switch ((SpvReflectDescriptorType)descriptorType)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return false;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return true;
        default:
            return false;
        }
    });
    env.add_callback("descriptor_type_string", 1, [](inja::Arguments& args) {
        const u32 descriptorType = args.at(0)->get<u32>();
        switch ((SpvReflectDescriptorType)descriptorType)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                return "DescriptorType::Sampler";
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return "DescriptorType::Image";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return "DescriptorType::ImageStorage";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:   return "DescriptorType::TexelUniform";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:   return "DescriptorType::TexelStorage";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return "DescriptorType::UniformBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return "DescriptorType::StorageBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "DescriptorType::UniformBufferDynamic";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "DescriptorType::StorageBufferDynamic";
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:       return "DescriptorType::Input";
        default: return "ERROR: Unknown descriptor type";
        }
    });
    env.add_callback("descriptors_kind_string", 1, [](inja::Arguments& args) {
        const u32 setIndex = args.at(0)->get<u32>();
        if (setIndex == 0)
            return "DescriptorsKind::Sampler";
        if (setIndex == 1)
            return "DescriptorsKind::Resource";
        if (setIndex == 2)
            return "DescriptorsKind::Materials";
        return "ERROR: Unknown set index";
    });

    nlohmann::json templateData = loadShaderInfos(shadersPath);
    generateTemplates(env, outputPath, templateData);
}
