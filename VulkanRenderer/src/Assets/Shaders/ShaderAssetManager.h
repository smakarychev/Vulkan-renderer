#pragma once

#include "Assets/AssetManager.h"
#include "ShaderAsset.h"
#include "Assets/Common/AssetSlotMap.h"
#include "Rendering/Shader/ShaderOverrides.h"
#include "Rendering/Shader/ShaderPipelineTemplate.h"

#include <AssetImportLib/Importers/ImportContext.h>
#include <AssetImportLib/Importers/Shaders/ShaderImporter.h>
#include <CoreLib/String/StringHeterogeneousHasher.h>
#include <CoreLib/String/StringId.h>

struct FrameContext;

namespace lux
{
namespace import
{
class ShaderImporter;
struct ShaderImportSettings;
}

template <>
struct ResourceAssetLoadParameters<ShaderAsset>
{
    using SourceType = StringId;
    SourceType ToSourceType() const { return Name; }
    
    StringId Name{};
    StringId Variant{import::ShaderBaker::MAIN_VARIANT};
    ShaderDefines Defines{};
    auto operator<=>(const ResourceAssetLoadParameters&) const = default;
};
using ShaderLoadParameters = ResourceAssetLoadParameters<ShaderAsset>;

template <>
struct ResourceAssetLoadParametersHasher<ShaderAsset>
{
    usize operator()(const ShaderLoadParameters& parameters) const noexcept
    {
        u64 hash = parameters.Name.Hash();
        Hash::combine(hash, parameters.Variant.Hash());
        Hash::combine(hash, parameters.Defines.Hash);
            
        return hash;
    }
};

class ShaderAssetManager final : public ResourceAssetManager<ShaderAsset, ResourceAssetTraits>
{
public:
    LUX_ASSET_MANAGER(ShaderAssetManager, "23ae71e5-8829-4643-a424-eb74e730d368"_guid)
    
    bool AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const AssetPath& path) override;
    
    void Init(const import::ShaderImportSettings& bakeSettings);
    void Shutdown();
    
    DescriptorsLayout GetTextureHeapLayout() const;
    DescriptorBinding GetTextureHeapBinding() const;
protected:
    ShaderHandle LoadAsset(const ShaderLoadParameters& parameters) override;
    void ReloadAsset(ShaderHandle handle) override;
    void UnloadAsset(ShaderHandle handle) override;
    const ShaderAsset* GetAsset(ShaderHandle handle) const override;
private:
    std::optional<ShaderAsset> DoLoad(import::ShaderImporter& importer, const AssetPath& path);
    std::vector<std::pair<std::string, std::string>> GetDefinesFromParameters(
        const ShaderLoadParameters& parameters) const;
    import::ShaderImportSettings CreateBakeSettings(const ShaderLoadParameters& parameters, 
        std::vector<std::pair<std::string, std::string>>& defines) const;
private:
    AssetSlotMap<ShaderAsset> m_ShaderPipelines;
    DescriptorsLayout m_TextureHeapLayout{};

    /* for hot-reloading */
    const import::ShaderImportSettings* m_BakeSettings{nullptr};
    StringUnorderedMap<std::vector<StringId>> m_RawPathToShaders;
    std::unordered_map<StringId, AssetPath> m_ShaderNameToAssetPath;
};
}
