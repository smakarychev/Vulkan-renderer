#pragma once

#include "Bakers/BakerContext.h"
#include "types.h"
#include "Containers/Result.h"
#include "String/StringId.h"
#include "v2/Shaders/ShaderLoadInfo.h"
#include "v2/Shaders/ShaderAsset.h"

namespace assetlib
{
struct ShaderLoadInfo;
}

namespace bakers
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
    
    static std::filesystem::path GetBakedPath(const std::filesystem::path& originalFile, StringId variant,
        const SlangBakeSettings& settings, const Context& ctx);

    IoResult<void> BakeVariantsToFile(const std::filesystem::path& path, const SlangBakeSettings& settings,
        const Context& ctx);
    
    IoResult<assetlib::ShaderAsset> BakeToFile(const std::filesystem::path& path, StringId variant,
        const SlangBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::ShaderAsset> Bake(const assetlib::ShaderLoadInfo& loadInfo,
        const assetlib::ShaderLoadInfo::Variant& variant, const SlangBakeSettings& settings, const Context& ctx);
private:
    static constexpr std::string_view POST_BAKE_EXTENSION = ".sl_shader";
};
}
