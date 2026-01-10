#include "types.h"
#include "core.h"

#include "Bakers/Bakers.h"

#include "Platform/PlatformUtils.h"
#include "Slang/SlangGenerator.h"
#include "Slang/SlangUniformTypeGenerator.h"
#include "utils/HashFileUtils.h"
#include "Utils/HashUtils.h"

#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

struct Config
{
    std::filesystem::path ShadersPath{};
    std::filesystem::path GenerationPath{};
    std::filesystem::path UniformSearchPath{};
};

template <> struct ::glz::meta<Config> : assetlib::reflection::CamelCase {};

std::optional<Config> readConfig(const std::filesystem::path& path)
{
    std::ifstream configFile(path, std::ios::binary | std::ios::ate);
    if (!configFile.good())
        return std::nullopt;
    const isize size = configFile.tellg();
    configFile.seekg(0, std::ios::beg);
    std::string buffer(size, 0);
    configFile.read(buffer.data(), size);
    configFile.close();

    Config config = {};
    if (const auto error = glz::read_json(config, buffer))
    {
        LOG("Failed to read config file: {} ({})", glz::format_error(error, buffer), path.string());
        return std::nullopt;
    }

    return config;
}

i32 main()
{
    fs::current_path(platform::getExecutablePath().parent_path());
    const fs::path configPath = "config.json";
    if (!fs::exists(configPath))
    {
        LOG("Failed to find config file: {}", configPath.string());
        return 1;
    }

    std::optional<Config> config = readConfig(configPath);
    if (!config)
        return 1;

    SlangUniformTypeGenerator uniformTypeGenerator;
    auto generationResult = uniformTypeGenerator.GenerateStandaloneUniforms({
        .SearchPath =  config->UniformSearchPath,
        .GenerationPath = config->GenerationPath,
        .TypesDirectoryName = "Types"
    });

    if (!generationResult.has_value())
    {
        LOG("SlangUniformTypeGenerator error: {}", generationResult.error());
    }
    
    SlangGenerator generator(uniformTypeGenerator, config->ShadersPath);
    {
        std::string commonFile = generator.GenerateCommonFile();
        const std::filesystem::path commonFilePath = generator.GetCommonFilePath(config->GenerationPath);
        const u32 existingHash = Hash::murmur3b32File(commonFilePath).value_or(0);
        const u32 newHash = Hash::murmur3b32((u8*)commonFile.data(), commonFile.size());
        if (existingHash != newHash)
        {
            std::ofstream out(commonFilePath.string(), std::ios::binary);
            out.write(commonFile.c_str(), (isize)commonFile.size());
        }
    }

    for (const auto& file : fs::recursive_directory_iterator(config->ShadersPath))
    {
        if (file.is_directory())
            continue;
        if (file.path().extension() != bakers::SHADER_ASSET_EXTENSION)
            continue;

        const assetlib::io::IoResult<SlangGeneratorResult> generated = generator.Generate(file.path());
        if (!generated.has_value())
        {
            LOG("Failed to generate bind group: {} ({})", generated.error(), file.path().string());
            continue;
        }

        std::filesystem::path generatedPath = config->GenerationPath / generated->FileName;
        const u32 existingHash = Hash::murmur3b32File(generatedPath).value_or(0);
        const u32 newHash = Hash::murmur3b32((const u8*)generated->Generated.data(), generated->Generated.size());
        if (existingHash != newHash)
        {
            std::ofstream out(generatedPath, std::ios::binary);
            out.write(generated->Generated.c_str(), (isize)generated->Generated.size());
            LOG("Generated bind group for {}", file.path().string());
        }
        else
            LOG("Skipped generating bind group for {}. No changes detected", file.path().string());
    }
}
