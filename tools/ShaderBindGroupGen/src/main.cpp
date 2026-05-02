
#include "Slang/SlangGenerator.h"
#include "Slang/SlangUniformTypeGenerator.h"

#include <AssetLib/Io/IoInterface/CombinedAssetIoInterface.h>
#include <AssetLib/Io/IoInterface/SeparateAssetIoInterface.h>
#include <AssetLib/Io/AssetIoRegistry.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <AssetBakerLib/Bakers/Bakers.h>
#include <AssetBakerLib/Bakers/BakerContext.h>

#include <CoreLib/core.h>
#include <CoreLib/types.h>
#include <CoreLib/Platform/PlatformUtils.h>
#include <CoreLib/utils/HashFileUtils.h>
#include <CoreLib/Utils/HashUtils.h>
#include <CoreLib/Utils/FileUtils.h>

#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

struct Config
{
    std::filesystem::path InitialDirectory{};
    std::filesystem::path BakedDirectory{};
    std::filesystem::path ShadersPath{};
    std::filesystem::path GenerationPath{};
    std::filesystem::path UniformSearchPath{};
    std::string IoInterfaceName;
    std::optional<lux::Guid> IoInterfaceGuid;
};

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

i32 main()
{
    using namespace lux::assetlib::io;
    lux::Logger::Init({.LoggerName = "BIND_GROUP_GEN"});
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

    AssetIoRegistry<AssetIoInterface> ioInterfaceRegistry;

    ioInterfaceRegistry.Register(
        SeparateAssetIoInterface::GetNameStatic(),
        SeparateAssetIoInterface::GetGuidStatic(),
        [](void*) { return std::make_shared<SeparateAssetIoInterface>(); });
    ioInterfaceRegistry.Register(
        CombinedAssetIoInterface::GetNameStatic(),
        CombinedAssetIoInterface::GetGuidStatic(),
        [](void*) { return std::make_shared<CombinedAssetIoInterface>(); });

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

    if (!io)
    {
        LUX_LOG_ERROR("Error: io context is not set");
        return 1;
    }

    SlangUniformTypeGenerator uniformTypeGenerator;
    auto generationResult = uniformTypeGenerator.GenerateStandaloneUniforms({
        .SearchPath =  config->UniformSearchPath,
        .GenerationPath = config->GenerationPath,
        .TypesDirectoryName = "Types"
    });

    if (!generationResult.has_value())
    {
        LUX_LOG_ERROR("SlangUniformTypeGenerator error: {}", generationResult.error());
    }
    auto context = std::make_shared<lux::bakers::Context>(lux::bakers::Context{
        .InitialDirectory = config->InitialDirectory,
        .BakedDirectory = config->BakedDirectory,
        .Io = io.get(),
        .Compressor = nullptr        
    });
    SlangGenerator generator(uniformTypeGenerator, context);
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
        if (file.path().extension() != lux::bakers::SHADER_ASSET_EXTENSION)
            continue;

        const IoResult<SlangGeneratorResult> generated = generator.Generate(file.path());
        if (!generated.has_value())
        {
            LUX_LOG_ERROR("Failed to generate bind group: {} ({})", generated.error(), file.path().string());
            continue;
        }

        std::filesystem::path generatedPath = config->GenerationPath / generated->FileName;
        const u32 existingHash = Hash::murmur3b32File(generatedPath).value_or(0);
        const u32 newHash = Hash::murmur3b32((const u8*)generated->Generated.data(), generated->Generated.size());
        if (existingHash != newHash)
        {
            std::ofstream out(generatedPath, std::ios::binary);
            out.write(generated->Generated.c_str(), (isize)generated->Generated.size());
            LUX_LOG_INFO("Generated bind group for {}", file.path().string());
        }
        else
            LUX_LOG_INFO("Skipped generating bind group for {}. No changes detected", file.path().string());
    }
}
