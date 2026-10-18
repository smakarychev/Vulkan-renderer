#pragma once

#include "Assets/AssetManager.h"
#include "ShaderAsset.h"
#include "Rendering/Shader/ShaderOverrides.h"
#include "Rendering/Shader/ShaderReflection.h"
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
    ShaderOverrides Overrides{};
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
        Hash::combine(hash, parameters.Overrides.Hash);
            
        return hash;
    }
};

enum class ShaderAssetManagerError : u8
{
    FailedToCreatePipeline,
    FailedToAllocateDescriptors,
};

struct ShaderTextureHeapAllocation
{
    Descriptors Descriptors{};
    PipelineLayout PipelineLayout{};
};

using ShaderCacheAllocateResult = std::expected<void, ShaderAssetManagerError>;
using ShaderCacheTextureHeapResult = std::expected<ShaderTextureHeapAllocation, ShaderAssetManagerError>;

class ShaderAssetManager final : public ResourceAssetManager<ShaderAsset, ResourceAssetTraitsGetOptional>
{
public:
    LUX_ASSET_MANAGER(ShaderAssetManager, "23ae71e5-8829-4643-a424-eb74e730d368"_guid)
    
    bool AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const AssetPath& path) override;
    
    void Init(const import::ShaderImportSettings& bakeSettings);
    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);
    ShaderCacheAllocateResult Allocate(ShaderHandle handle, DescriptorArenaAllocators& allocators);
    ShaderCacheTextureHeapResult AllocateTextureHeap(DescriptorArenaAllocator persistentAllocator, u32 count);
protected:
    ShaderHandle LoadAsset(const ShaderLoadParameters& parameters) override;
    void ReloadAsset(ShaderHandle handle) override;
    void UnloadAsset(ShaderHandle handle) override;
    GetType GetAsset(ShaderHandle handle) const override;
private:
    struct LoadedPipelineInfo
    {
        const ShaderPipelineTemplate* PipelineTemplate{nullptr};
        PipelineLayout Layout{};
        bool HasTextureHeap{};
    };
    struct PipelineInfo
    {
        const ShaderPipelineTemplate* PipelineTemplate{nullptr};
        Pipeline Pipeline{};
        PipelineLayout Layout{};
        bool HasTextureHeap{};
        std::array<::Descriptors, MAX_DESCRIPTOR_SETS> Descriptors{};
        std::array<::DescriptorsLayout, MAX_DESCRIPTOR_SETS> DescriptorLayouts{};
        StringId Name{};
    };
    std::optional<LoadedPipelineInfo> DoLoad(import::ShaderImporter& importer, const AssetPath& path);
    Pipeline CreatePipeline(const ShaderPipelineTemplate& pipelineTemplate, const ShaderLoadParameters& parameters);
    void ReloadPipeline(PipelineInfo& pipelineInfo, const ShaderLoadParameters& parameters);
    std::vector<std::pair<std::string, std::string>> GetDefinesFromParameters(
        const ShaderLoadParameters& parameters) const;
    import::ShaderImportSettings CreateBakeSettings(const ShaderLoadParameters& parameters, 
        std::vector<std::pair<std::string, std::string>>& defines) const;
private:
    std::vector<PipelineInfo> m_Pipelines;
    std::unordered_map<assetlib::AssetId, ShaderPipelineTemplate> m_ShaderPipelineTemplates;

    struct DescriptorsWithLayout
    {
        Descriptors Descriptors{};
        DescriptorsLayout Layout{};
    };
    DescriptorsWithLayout m_TextureHeap{};

    /* for hot-reloading */
    const import::ShaderImportSettings* m_BakeSettings{nullptr};
    StringUnorderedMap<std::vector<StringId>> m_RawPathToShaders;
    std::unordered_map<StringId, AssetPath> m_ShaderNameToAssetPath;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
