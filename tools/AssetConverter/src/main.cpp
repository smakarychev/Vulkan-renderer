#include "AssetConverter.h"
#include "Log.h"
#include "Bakers/BakerContext.h"
#include "Bakers/Bakers.h"
#include "Bakers/BakersDispatcher.h"
#include "Bakers/Images/ImageBaker.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "Platform/PlatformUtils.h"
#include "v2/Io/AssetIoRegistry.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"
#include "v2/Io/Compression/AssetCompressor.h"
#include "v2/Io/Compression/Lz4AssetCompressor.h"
#include "v2/Io/Compression/RawAssetCompressor.h"
#include "v2/Io/IoInterface/CombinedAssetIoInterface.h"
#include "v2/Io/IoInterface/SeparateAssetIoInterface.h"
#include "v2/Reflection/AssetLibReflectionUtility.inl"

#include <filesystem>
#include <memory>
#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

struct ShaderBakerSettings
{
    std::filesystem::path IncludeDirectory{};
    std::string ShadersDirectoryName{"shaders/"};
    std::string IoInterfaceName{};
    std::string IoCompressorName{};
};

struct Config
{
    std::filesystem::path InitialDirectory{};
    std::optional<ShaderBakerSettings> ShaderBakerSettings{std::nullopt};
    std::string IoInterfaceName;
    std::optional<lux::Guid> IoInterfaceGuid;
    std::string IoCompressorName;
    std::optional<lux::Guid> IoCompressorGuid;
};

template <> struct ::glz::meta<ShaderBakerSettings> : lux::assetlib::reflection::CamelCase {}; 
template <> struct ::glz::meta<Config> : lux::assetlib::reflection::CamelCase {};

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
        LUX_LOG_ERROR("Failed to read config file: {} ({})", glz::format_error(error, buffer), path.string());
        return std::nullopt;
    }

    return config;
}

i32 main(i32 argc, char** argv)
{
    using namespace lux::assetlib::io;
    lux::Logger::Init({});
    fs::current_path(platform::getExecutablePath().parent_path());
    const fs::path configPath = "config.json";
    if (!fs::exists(configPath))
    {
        LUX_LOG_ERROR("Failed to find config file: {}", configPath.string());
        return 1;
    }

    std::optional<Config> config = readConfig(configPath);
    if (!config)
        return 1;

    StringIdRegistry::Init();

    AssetIoRegistry<AssetIoInterface> ioInterfaceRegistry;
    AssetIoRegistry<AssetCompressor> ioCompressorRegistry;

    ioInterfaceRegistry.Register(
        SeparateAssetIoInterface::GetNameStatic(),
        SeparateAssetIoInterface::GetGuidStatic(),
        [](void*) { return std::make_shared<SeparateAssetIoInterface>(); });
    ioInterfaceRegistry.Register(
        CombinedAssetIoInterface::GetNameStatic(),
        CombinedAssetIoInterface::GetGuidStatic(),
        [](void*) { return std::make_shared<CombinedAssetIoInterface>(); });
    
    ioCompressorRegistry.Register(
        RawAssetCompressor::GetNameStatic(),
        RawAssetCompressor::GetGuidStatic(),
        [](void*) { return std::make_shared<RawAssetCompressor>(); });
    ioCompressorRegistry.Register(
        Lz4AssetCompressor::GetNameStatic(),
        Lz4AssetCompressor::GetGuidStatic(),
        [](void*) { return std::make_shared<Lz4AssetCompressor>(); });

    std::shared_ptr<AssetIoInterface> io;
    if (config->IoInterfaceGuid.has_value())
    {
        io = ioInterfaceRegistry.Create(config->IoInterfaceName, *config->IoInterfaceGuid).value_or(nullptr);
    }
    else
    {
        auto createResult = ioInterfaceRegistry.Create(config->IoInterfaceName);
        if (!createResult.has_value())
        {
            if (createResult.error() == AssetIoRegistryCreateError::Ambiguous)
                LUX_LOG_ERROR("Ambiguous io interface name, guid is necessary");
        }
        io = createResult.value_or(nullptr);
    }
    std::shared_ptr<AssetCompressor> compressor;
    if (config->IoCompressorGuid.has_value())
    {
        compressor = ioCompressorRegistry.Create(config->IoCompressorName, *config->IoCompressorGuid).value_or(nullptr);
    }
    else
    {
        auto createResult = ioCompressorRegistry.Create(config->IoCompressorName);
        if (!createResult.has_value())
        {
            if (createResult.error() == AssetIoRegistryCreateError::Ambiguous)
                LUX_LOG_ERROR("Ambiguous io compressor name, guid is necessary");
        }
        compressor = createResult.value_or(nullptr);
    }
    
    auto shaderBakerSettings = config->ShaderBakerSettings.value_or(ShaderBakerSettings{});
    lux::bakers::Context bakerContext{
        .InitialDirectory = config->InitialDirectory,
        .Io = io.get(),
        .Compressor = compressor.get()
    };
    lux::bakers::SlangBakeSettings shaderBakeSettings{
        .IncludePaths = {shaderBakerSettings.IncludeDirectory.string()},
    };
    lux::bakers::ImageBakeSettings imageBakeSettings{};

    if (!bakerContext.Io)
    {
        LUX_LOG_ERROR("io context is not set");
        return 1;
    }
    if (!bakerContext.Compressor)
    {
        LUX_LOG_ERROR("io compressor is not set");
        return 1;
    }
    
    for (const auto& file : fs::recursive_directory_iterator(config->InitialDirectory))
    {
        if (file.is_directory())
            continue;

        lux::bakers::BakersDispatcher dispatcher(file);
        
        dispatcher.Dispatch({lux::bakers::SHADER_ASSET_EXTENSION}, [&](const fs::path& path) {
            lux::bakers::Slang baker;
            if (!baker.ShouldBake(path, shaderBakeSettings, bakerContext))
                return;
            
            auto baked = baker.BakeVariantsToFile(path, shaderBakeSettings, bakerContext);
            if (!baked)
                LUX_LOG_ERROR("Failed to bake shader file: {} ({})", baked.error(), path.string());
            else
                LUX_LOG_INFO("Baked shader file: {}", path.string());
        });
        dispatcher.Dispatch(lux::bakers::IMAGE_ASSET_RAW_EXTENSIONS, [&](const fs::path& path) {
            lux::bakers::ImageBaker baker;
            if (!baker.ShouldBake(path, imageBakeSettings, bakerContext))
                return;
            
            auto baked = baker.BakeToFile(path, imageBakeSettings, bakerContext);
            if (!baked)
                LUX_LOG_ERROR("Failed to bake image file: {} ({})", baked.error(), path.string());
            else
                LUX_LOG_INFO("Baked image file: {}", path.string());
        });
    }
    
    if (argc < 2)
    {
        LUX_LOG_INFO("Usage: AssetConverter <directory>");
        return 1;
    }

    AssetConverter::BakeDirectory(std::filesystem::weakly_canonical(argv[1]));
}
