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
    StringId Name{};
    std::optional<StringId> Variant{};
    ShaderOverridesView* Overrides{nullptr};
};
using ShaderLoadParameters = ResourceAssetLoadParameters<ShaderAsset>;

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
    
    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const std::filesystem::path& path) override;
    
    void Init(const import::ShaderImportSettings& bakeSettings);
    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);
    ShaderCacheAllocateResult Allocate(ShaderHandle handle, DescriptorArenaAllocators& allocators);
    ShaderCacheTextureHeapResult AllocateTextureHeap(DescriptorArenaAllocator persistentAllocator, u32 count);
protected:
    ShaderHandle LoadAsset(const ShaderLoadParameters& parameters) override;
    void UnloadAsset(ShaderHandle handle) override;
    GetType GetAsset(ShaderHandle handle) const override;
private:
    void OnRawFileModified(const std::filesystem::path& path);
    
    struct PipelineInfo
    {
        const ShaderPipelineTemplate* PipelineTemplate{nullptr};
        Pipeline Pipeline{};
        PipelineLayout Layout{};
        bool HasTextureHeap{};
        std::array<::Descriptors, MAX_DESCRIPTOR_SETS> Descriptors{};
        std::array<::DescriptorsLayout, MAX_DESCRIPTOR_SETS> DescriptorLayouts{};
        StringId Name{};

        bool ShouldReload{false};
    };
    struct ShaderNameWithOverrides
    {
        StringId Name{};
        StringId Variant{};
        u64 OverridesHash{};

        auto operator<=>(const ShaderNameWithOverrides&) const = default;
    };
    struct RebakeInfo
    {
        ShaderNameWithOverrides NameWithOverrides{};
        u64 DefinesHash{0};
        std::vector<std::pair<std::string, std::string>> Defines{};

        auto operator<=>(const RebakeInfo&) const = default;
    };
    std::optional<PipelineInfo> DoLoad(import::ShaderImporter& importer, const std::filesystem::path& path);
    Pipeline CreatePipeline(const assetlib::ShaderLoadInfo& shaderLoadInfo, const PipelineInfo& pipelineInfo, 
        const ShaderLoadParameters& parameters);
    void ReloadPipeline(PipelineInfo& pipelineInfo, import::ShaderImporter& importer, 
        const ShaderLoadParameters& parameters);
    RebakeInfo CreateRebakeInfo(const ShaderNameWithOverrides& name, const ShaderLoadParameters& parameters) const;
    import::ShaderImportSettings CreateBakeSettings(const RebakeInfo& rebakeInfo) const;
private:
    struct ShaderNameWithOverridesHasher
    {
        constexpr u64 operator()(const ShaderNameWithOverrides& shader) const
        {
            u64 hash = shader.Name.Hash();
            Hash::combine(hash, shader.Variant.Hash());
            Hash::combine(hash, shader.OverridesHash);
            
            return hash;
        }
    };

    std::vector<PipelineInfo> m_Pipelines;
    std::unordered_map<ShaderNameWithOverrides, ShaderHandle, ShaderNameWithOverridesHasher> m_PipelinesMap;
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
    std::unordered_map<StringId, std::filesystem::path> m_ShaderNameToRawPath;
    StringUnorderedMap<std::vector<RebakeInfo>> m_RawPathToRebakeInfos;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
