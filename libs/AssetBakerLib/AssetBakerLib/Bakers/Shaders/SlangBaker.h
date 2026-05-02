#pragma once

#include <AssetLib/Shaders/ShaderLoadInfo.h>
#include <AssetLib/Shaders/ShaderMeta.h>
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
    StringId Variant{};
    std::vector<std::string> IncludePaths{};
    std::string UniformReflectionDirectoryName{"uniform_types"};
    bool EnableHotReloading{false};
};

class Slang
{
public:
    static constexpr auto MAIN_VARIANT = HashedStringView(assetlib::ShaderLoadInfo::SHADER_VARIANT_MAIN_NAME);
    static constexpr std::string_view POST_BAKE_EXTENSION = ".sl_shader";
public:
    Slang(const std::shared_ptr<Context>& ctx, const SlangBakeSettings& settings) : m_Ctx(ctx), m_Settings(settings) {}
    
    std::filesystem::path GetBakedPath(const std::filesystem::path& metaPath) const;
    std::filesystem::path GetBakedPath(const assetlib::ShaderMeta& meta) const;
    
    IoResult<std::filesystem::path> BakeToFile(assetlib::ShaderMeta& meta, const std::filesystem::path& metaPath);

    bool ShouldBake(const std::filesystem::path& metaPath) const;
    
    std::optional<u64> GetDefinesHash(const assetlib::ShaderLoadInfo& loadInfo) const;
    std::filesystem::path GetDefineAwarePath(const std::filesystem::path& path, u64 definesHash) const;
private:
    IoResult<assetlib::ShaderAsset> Bake(const assetlib::ShaderMeta& meta, const assetlib::ShaderLoadInfo& loadInfo);
private:
    std::shared_ptr<Context> m_Ctx{nullptr};
    SlangBakeSettings m_Settings{};
};
}
