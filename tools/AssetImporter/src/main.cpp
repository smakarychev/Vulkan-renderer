
#include <AssetLib/Io/AssetIoRegistry.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/Compression/Lz4AssetCompressor.h>
#include <AssetLib/Io/Compression/RawAssetCompressor.h>
#include <AssetLib/Io/IoInterface/CombinedAssetIoInterface.h>
#include <AssetLib/Io/IoInterface/SeparateAssetIoInterface.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <AssetImportLib/Importers/ImportContext.h>
#include <AssetImportLib/Importers/Import.h>
#include <AssetImportLib/Bakers/BakersDispatcher.h>
#include <AssetImportLib/Importers/Images/ImageImporter.h>
#include <AssetImportLib/Importers/Scenes/SceneImporter.h>
#include <AssetImportLib/Importers/Shaders/ShaderImporter.h>
#include <CoreLib/Log.h>
#include <CoreLib/Platform/PlatformUtils.h>
#include <CoreLib/Utils/FileUtils.h>

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
    std::filesystem::path BakedDirectory{};
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
    auto read = lux::readFileToString(path);
    if (!read.has_value())
        return std::nullopt;

    Config config = {};
    if (const auto error = glz::read_json(config, *read))
    {
        LUX_LOG_ERROR("Failed to read config file: {} ({})", glz::format_error(error, *read), path.string());
        return std::nullopt;
    }

    return config;
}

i32 main(i32 argc, char** argv)
{
    using namespace lux::assetlib::io;
    lux::Logger::Init({.LoggerName = "ASSET_IMPORTER"});
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
    auto importContext = std::make_shared<lux::import::Context>(lux::import::Context{
        .InitialDirectory = config->InitialDirectory,
        .BakedDirectory = config->BakedDirectory,
        .Io = io.get(),
        .Compressor = compressor.get()
    });

    if (!importContext->Io)
    {
        LUX_LOG_ERROR("io context is not set");
        return 1;
    }
    if (!importContext->Compressor)
    {
        LUX_LOG_ERROR("io compressor is not set");
        return 1;
    }
    
    for (const auto& file : fs::recursive_directory_iterator(config->InitialDirectory))
    {
        if (file.is_directory())
            continue;

        lux::import::BakersDispatcher dispatcher(file);
        
        dispatcher.Dispatch({lux::import::SHADER_ASSET_LOAD_EXTENSION}, [&](const fs::path& path) {
            auto shaderLoadInfoRead = lux::assetlib::shader::readLoadInfo(path);
            if (!shaderLoadInfoRead.has_value())
                return;
            
            for (auto& variant : shaderLoadInfoRead->Variants)
            {
                lux::import::ShaderImportSettings shaderImportSettings{
                    .Variant = StringId::FromString(variant.Name),
                    .IncludePaths = {shaderBakerSettings.IncludeDirectory.string()},
                };
                
                lux::import::ShaderImporter importer(importContext, shaderImportSettings);
                if (!importer.NeedsBaking(path))
                    continue;
                
                auto imported = importer.Import(path);
                if (!imported)
                    LUX_LOG_ERROR("Failed to import shader file: {} ({})", imported.error(), path.string());
                else
                    LUX_LOG_INFO("Imported shader file: {}", path.string());
            }
        });
        dispatcher.Dispatch({lux::import::SCENE_ASSET_RAW_EXTENSIONS}, [&](const fs::path& path) {
            lux::import::SceneImporter importer(importContext);
            if (!importer.NeedsBaking(path))
                return;

            auto imported = importer.Import(path);
            if (!imported)
                LUX_LOG_ERROR("Failed to import scene file: {} ({})", imported.error(), path.string());
            else
                LUX_LOG_INFO("Imported scene file: {}", path.string());
        });
        dispatcher.Dispatch(lux::import::IMAGE_ASSET_RAW_EXTENSIONS, [&](const fs::path& path) {
            lux::import::ImageImporter importer(importContext, {});
            if (!importer.NeedsBaking(path))
                return;
            
            auto imported = importer.Import(path);
            if (!imported)
                LUX_LOG_ERROR("Failed to import image file: {} ({})", imported.error(), path.string());
            else
                LUX_LOG_INFO("Imported image file: {}", path.string());
        });
    }
}
