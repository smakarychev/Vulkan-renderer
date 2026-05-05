#pragma once

#include <AssetLib/Shaders/ShaderAsset.h>
#include <AssetLib/Shaders/ShaderMeta.h>
#include <AssetImportLib/Importers/Importer.h>
#include <AssetImportLib/Bakers/Shaders/ShaderBaker.h>
#include <CoreLib/Containers/Span.h>

namespace lux::import
{
struct ShaderImportedAsset : ImportedAsset
{
    assetlib::ShaderAsset Asset{};
};

struct ShaderImportSettings
{
    Span<const std::pair<std::string, std::string>> Defines{};
    u64 DefinesHash{0};
    StringId Variant{};
    std::vector<std::string> IncludePaths{};
    std::string UniformReflectionDirectoryName{"uniform_types"};
    bool EnableHotReloading{false};
};

class ShaderImporter final : public Importer
{
public:
    ShaderImporter(const std::shared_ptr<Context>& ctx, const ShaderImportSettings& settings);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedShader(); }
    const ShaderImportedAsset& GetImportedShader() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::ShaderMeta& GetImportedShaderMetadata() const { return m_ImportedMeta; }
    const assetlib::ShaderLoadInfo& GetImportedShaderLoadInfo() const { return m_ImportedLoadInfo; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override;
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
protected:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath) override;
private:
    ShaderImportedAsset m_ImportedAsset{};
    assetlib::ShaderMeta m_ImportedMeta{};
    assetlib::ShaderLoadInfo m_ImportedLoadInfo{};
    ShaderImportSettings m_ImportSettings{};
    ShaderBaker m_Baker;
};
}