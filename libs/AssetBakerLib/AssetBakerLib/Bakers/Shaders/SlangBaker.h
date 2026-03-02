#pragma once

#include <AssetLib/Shaders/ShaderLoadInfo.h>
#include <AssetLib/Shaders/ShaderAsset.h>
#include <AssetBakerLib/Bakers/BakerContext.h>
#include <CoreLib/types.h>
#include <CoreLib/Containers/Result.h>
#include <CoreLib/Containers/Span.h>
#include <CoreLib/String/StringId.h>

namespace lux::assetlib
{
struct ShaderLoadInfo;
}

namespace lux::bakers
{
struct SlangBakeSettings
{
    Span<const std::pair<std::string, std::string>> Defines{};
    u64 DefinesHash{0};
    std::vector<std::string> IncludePaths;
    std::string UniformReflectionDirectoryName{"uniform_types"};
    bool EnableHotReloading{false};
};

class Slang
{
public:
    static constexpr auto MAIN_VARIANT = HashedStringView(assetlib::ShaderLoadInfo::SHADER_VARIANT_MAIN_NAME);
    static constexpr std::string_view POST_BAKE_EXTENSION = ".sl_shader";
public:
    static std::filesystem::path GetBakedPath(const std::filesystem::path& originalFile, StringId variant,
        const SlangBakeSettings& settings, const Context& ctx);

    IoResult<void> BakeVariantsToFile(const std::filesystem::path& path, const SlangBakeSettings& settings,
        const Context& ctx);
    
    IoResult<std::filesystem::path> BakeToFile(const std::filesystem::path& path, StringId variant,
        const SlangBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::ShaderAsset> Bake(const assetlib::ShaderLoadInfo& loadInfo,
        const assetlib::ShaderLoadInfo::Variant& variant, const SlangBakeSettings& settings, const Context& ctx);

    bool ShouldBake(const std::filesystem::path& path, const SlangBakeSettings& settings,
        const Context& ctx);
};
}
