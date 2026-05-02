#pragma once

#include <AssetLib/Shaders/ShaderAsset.h>
#include <AssetLib/Shaders/ShaderMeta.h>
#include <AssetBakerLib/Importers/Importer.h>
#include <AssetBakerLib/Bakers/Shaders/SlangBaker.h>

namespace lux::bakers
{
struct ShaderImportedAsset : ImportedAsset
{
    assetlib::ShaderAsset Asset{};
};

class ShaderImporter final : public Importer
{
public:
    ShaderImporter(const std::shared_ptr<Context>& ctx, const SlangBakeSettings& settings);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedShader(); }
    const ShaderImportedAsset& GetImportedShader() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::ShaderMeta& GetImportedShaderMetadata() const { return m_ImportedMeta; }
    const assetlib::ShaderLoadInfo& GetImportedShaderLoadInfo() const { return m_ImportedLoadInfo; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override;
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
private:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath);
private:
    ShaderImportedAsset m_ImportedAsset{};
    assetlib::ShaderMeta m_ImportedMeta{};
    assetlib::ShaderLoadInfo m_ImportedLoadInfo{};
    Slang m_Baker;
    SlangBakeSettings m_BakeSettings{};
};
}