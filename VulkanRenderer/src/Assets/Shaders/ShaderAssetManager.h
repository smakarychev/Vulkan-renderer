#pragma once

#include "Assets/AssetManager.h"
#include "Bakers/BakerContext.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "Rendering/Pipeline.h"
#include "Rendering/Shader/ShaderOverrides.h"
#include "Rendering/Shader/ShaderReflection.h"
#include "Rendering/Shader/ShaderPipelineTemplate.h"
#include "String/StringHeterogeneousHasher.h"
#include "String/StringId.h"

struct FrameContext;

namespace lux
{

namespace bakers
{
struct SlangBakeSettings;
}

class Shader
{
    friend class ShaderAssetManager;
public:
    Pipeline Pipeline() const { return m_Pipeline; }
    PipelineLayout GetLayout() const { return m_PipelineLayout; }
    const ::Descriptors& Descriptors(u32 index) const { return m_Descriptors[index]; }
    const ::DescriptorsLayout& DescriptorsLayout(u32 index) const { return m_DescriptorLayouts[index]; }
private:
    ::Pipeline m_Pipeline{};
    PipelineLayout m_PipelineLayout{};
    std::array<::Descriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    std::array<::DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorLayouts;
};
static_assert(std::is_trivially_destructible_v<Shader>);

template <>
struct ResourceAssetLoadParameters<Shader>
{
    StringId Name{};
    std::optional<StringId> Variant{};
    ShaderOverridesView* Overrides{nullptr};
};
using ShaderLoadParameters = ResourceAssetLoadParameters<Shader>;

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

using ShaderHandle = AssetHandle<Shader>;

class ShaderAssetManager final : public ResourceAssetManager<Shader, ResourceAssetTraitsGetOptional>
{
public:
    LUX_ASSET_MANAGER(ShaderAssetManager, "23ae71e5-8829-4643-a424-eb74e730d368"_guid)
    
    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Bakes(const std::filesystem::path& path) override;
    void OnFileModified(const std::filesystem::path& path) override;
    
    void Init(const bakers::SlangBakeSettings& bakeSettings);
    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);
    ShaderCacheAllocateResult Allocate(ShaderHandle handle, DescriptorArenaAllocators& allocators);
    ShaderCacheTextureHeapResult AllocateTextureHeap(DescriptorArenaAllocator persistentAllocator, u32 count);
protected:
    ShaderHandle LoadAsset(const ShaderLoadParameters& parameters) override;
    void UnloadAsset(ShaderHandle handle) override;
    GetType GetAsset(ShaderHandle handle) const override;
private:
    bool LoadShaderInfo(const std::filesystem::path& path, AssetIdResolver& resolver);
    void OnBakedFileModified(const std::filesystem::path& path);
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
    Result<std::filesystem::path, IoError> Bake(const ShaderLoadParameters& parameters,
        const ShaderNameWithOverrides& name);
    std::optional<PipelineInfo> TryCreatePipeline(const ShaderLoadParameters& parameters,
        const ShaderNameWithOverrides& name);
    const ShaderPipelineTemplate* GetShaderPipelineTemplate(const ShaderLoadParameters& parameters,
        const ShaderNameWithOverrides& name,
        const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides);
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
    std::unordered_map<ShaderNameWithOverrides, ShaderPipelineTemplate, ShaderNameWithOverridesHasher>
        m_ShaderPipelineTemplates;

    struct DescriptorsWithLayout
    {
        Descriptors Descriptors{};
        DescriptorsLayout Layout{};
    };
    DescriptorsWithLayout m_TextureHeap{};

    /* for hot-reloading */
    bakers::Context m_Context{};
    const bakers::SlangBakeSettings* m_BakeSettings{nullptr};
    StringUnorderedMap<std::vector<StringId>> m_RawPathToShaders;
    std::unordered_map<StringId, std::filesystem::path> m_ShaderNameToRawPath;
    StringUnorderedMap<ShaderNameWithOverrides> m_BakedPathToShaderName;
    struct RebakeInfo
    {
        StringId Variant{};
        std::vector<std::pair<std::string, std::string>> Defines{};
        u64 DefinesHash{0};

        auto operator<=>(const RebakeInfo&) const = default;
    };
    StringUnorderedMap<std::vector<RebakeInfo>> m_RawPathToRebakeInfos;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
