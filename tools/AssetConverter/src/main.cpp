#include "AssetConverter.h"
#include "Bakers/BakerContext.h"
#include "Bakers/Bakers.h"
#include "Bakers/BakersDispatcher.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "Platform/PlatformUtils.h"

#include <filesystem>
#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

struct ShaderBakerSettings
{
    std::filesystem::path IncludeDirectory{};
    std::string ShadersDirectoryName{"shaders/"};
};

struct Config
{
    std::filesystem::path InitialDirectory{};
    std::optional<ShaderBakerSettings> ShaderBakerSettings{std::nullopt};
};

template <> struct ::glz::meta<ShaderBakerSettings> : assetlib::reflection::CamelCase {}; 
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

i32 main(i32 argc, char** argv)
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

    auto shaderBakerSettings = config->ShaderBakerSettings.value_or(ShaderBakerSettings{});
    bakers::Context bakerContext{
        .InitialDirectory = config->InitialDirectory / shaderBakerSettings.ShadersDirectoryName,
    };
    bakers::SlangBakeSettings shaderBakeSettings{
        .IncludePaths = {shaderBakerSettings.IncludeDirectory.string()},
    };
    for (const auto& file : fs::recursive_directory_iterator(config->InitialDirectory))
    {
        if (file.is_directory())
            continue;

        bakers::BakersDispatcher dispatcher(file);
        
        dispatcher.Dispatch({bakers::SHADER_ASSET_EXTENSION}, [&](const fs::path& path) {
            bakers::Slang baker;
            auto baked = baker.BakeToFile(path, shaderBakeSettings, bakerContext);
            if (!baked)
                LOG("Failed to bake file: {} ({})", baked.error(), path.string());
            else
                LOG("Baked shader file: {}", path.string());
        });
    }
    
    if (argc < 2)
    {
        LOG("Usage: AssetConverter <directory>");
        return 1;
    }

    AssetConverter::BakeDirectory(std::filesystem::weakly_canonical(argv[1]));
}
