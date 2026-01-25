#include "types.h"
#include "core.h"

#include "Bakers/Bakers.h"

#include "Platform/PlatformUtils.h"
#include "Slang/SlangGenerator.h"
#include "Slang/SlangUniformTypeGenerator.h"
#include "utils/HashFileUtils.h"
#include "Utils/HashUtils.h"
#include "v2/Io/IoInterface/CombinedAssetIoInterface.h"
#include "v2/Io/IoInterface/SeparateAssetIoInterface.h"
#include "v2/Io/AssetIoRegistry.h"
#include "v2/Reflection/AssetLibReflectionUtility.inl"

#include <glaze/glaze.hpp>

namespace fs = std::filesystem;

struct Config
{
    std::filesystem::path ShadersPath{};
    std::filesystem::path GenerationPath{};
    std::filesystem::path UniformSearchPath{};
    std::string IoInterfaceName;
    std::optional<lux::Guid> IoInterfaceGuid;
};

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
        LOG("Failed to read config file: {} ({})", glz::format_error(error, buffer), path.string());
        return std::nullopt;
    }

    return config;
}

i32 main()
{
    using namespace lux::assetlib::io;
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
                LOG("Ambiguous io interface name, guid is necessary");
        }
        io = createResult.value_or(nullptr);
    }

    if (!io)
    {
        LOG("Error: io context is not set");
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
        LOG("SlangUniformTypeGenerator error: {}", generationResult.error());
    }
    
    SlangGenerator generator(uniformTypeGenerator, config->ShadersPath, *io);
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
