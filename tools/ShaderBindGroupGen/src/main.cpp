#include "types.h"
#include "core.h"

#include "ShaderAsset.h"

#include <inja/inja.hpp>
#include <fstream>
#include <spirv_reflect.h>
#include <unordered_set>

#include "Platform/PlatformUtils.h"

namespace fs = std::filesystem;

static const fs::path TEMPLATE_PATH = "./templates/";
static constexpr std::string_view BIND_GROUP_TEMPLATE = "bindgroups.tmpl";
static constexpr std::string_view BIND_GROUP_GENERATED_NAME = "ShaderBindGroups.generated.h";
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
        
        auto& stages = shader["shader_stages"];
        for (const auto& stage : stages)
        {
            assetLib::File shaderFile;
            assetLib::loadAssetFile(fs::weakly_canonical(shadersPath / std::string{stage}).string(), shaderFile);

            nlohmann::json stageJson = nlohmann::json::parse(shaderFile.JSON);
            for (const auto& set : stageJson["descriptor_sets"])
            {
                u32 setIndex = set["set"];
                for (const auto& binding : set["bindings"])
                {
                    std::string name = binding["name"];
                    if (processedBindings.contains(name))
                        continue;

                    processedBindings.emplace(name);

                    nlohmann::json bindingJson = {};
                    bindingJson["name"] = canonicalizeBindingName(name);
                    bindingJson["set"] = setIndex;
                    bindingJson["binding"] = (u32)binding["binding"];
                    bindingJson["count"] = (u32)binding["count"];
                    bindingJson["descriptor"] = (u32)binding["descriptor"];
                    shaderTemplateData["bindings"].emplace_back(bindingJson);
                }
            }
        }

        templateData["shaders"].emplace_back(shaderTemplateData);
    }

    return templateData;
}

void generateTemplates(inja::Environment& env, const fs::path& outputPath, const nlohmann::json& data)
{
    inja::Template bindgroupTemplate = env.parse_template((TEMPLATE_PATH / BIND_GROUP_TEMPLATE).string());

    std::ofstream out(outputPath / BIND_GROUP_GENERATED_NAME);
    env.render_to(out, bindgroupTemplate, data);
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
            return "ShaderDescriptorsKind::Sampler";
        if (setIndex == 1)
            return "ShaderDescriptorsKind::Resource";
        if (setIndex == 2)
            return "ShaderDescriptorsKind::Materials";
        return "ERROR: Unknown set index";
    });
    generateTemplates(env, outputPath, loadShaderInfos(shadersPath));
}
