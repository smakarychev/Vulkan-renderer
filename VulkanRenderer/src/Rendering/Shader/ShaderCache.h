#pragma once

#include "FrameContext.h"
#include "ShaderPipelineTemplate.h"
#include "ShaderOverrides.h"
#include "Platform/FileWatcher.h"
#include "String/StringHeterogeneousHasher.h"
#include "String/StringId.h"

#include <mutex>

namespace bakers
{
struct SlangBakeSettings;
struct Context;
}

class Shader
{
    friend class ShaderCache;
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

enum class ShaderCacheError
{
    FailedToCreatePipeline,
    FailedToAllocateDescriptors,
};
enum class ShaderCacheAllocationType : u8
{
    Pipeline    = BIT(0),
    Descriptors = BIT(1),

    Complete = Pipeline | Descriptors,
};
CREATE_ENUM_FLAGS_OPERATORS(ShaderCacheAllocationType)

using ShaderCacheAllocateResult = std::expected<Shader, ShaderCacheError>;

class ShaderCache
{
public:
    void Init(bakers::Context& bakersCtx, const bakers::SlangBakeSettings& bakeSettings);
    void Shutdown();
    void OnFrameBegin(FrameContext& ctx);
    
    ShaderCacheAllocateResult Allocate(StringId name, DescriptorArenaAllocators& allocators,
        ShaderCacheAllocationType allocationType = ShaderCacheAllocationType::Complete);
    ShaderCacheAllocateResult Allocate(StringId name, std::optional<StringId> variant, ShaderOverridesView&& overrides,
        DescriptorArenaAllocators& allocators,
        ShaderCacheAllocationType allocationType = ShaderCacheAllocationType::Complete);
    void AddPersistentDescriptors(StringId name, Descriptors descriptors, DescriptorsLayout descriptorsLayout);
private:
    void InitFileWatcher();
    void LoadShaderInfos();
    void LoadShaderInfo(const std::filesystem::path& path);

    void HandleModifications();
    void HandleStageModification(const std::filesystem::path& path);
    void MarkOverridesToReload(StringId name);

    struct PipelineInfo
    {
        const ShaderPipelineTemplate* PipelineTemplate{nullptr};
        Pipeline Pipeline{};
        PipelineLayout Layout{};
        StringId BindlessName{};
        u32 BindlessCount{0};
        bool ShouldReload{false};
    };
    struct ShaderNameWithOverrides
    {
        StringId Name{};
        StringId Variant{};
        u64 OverridesHash{};

        auto operator<=>(const ShaderNameWithOverrides&) const = default;
    };
    std::optional<PipelineInfo> TryCreatePipeline(const ShaderNameWithOverrides& name, ShaderOverridesView& overrides,
        ShaderCacheAllocationType allocationHint);
    const ShaderPipelineTemplate* GetShaderPipelineTemplate(const ShaderNameWithOverrides& name,
        const ShaderOverridesView& overrides, const std::filesystem::path& path,
        const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides);
private:
    struct ShaderNameWithOverridesHasher
    {
        constexpr u64 operator()(ShaderNameWithOverrides shader) const
        {
            u64 hash = shader.Name.Hash();
            Hash::combine(hash, shader.Variant.Hash());
            Hash::combine(hash, shader.OverridesHash);
            
            return hash;
        }
    };
    struct DescriptorsWithLayout
    {
        Descriptors Descriptors{};
        DescriptorsLayout Layout{};
    };
    std::unordered_map<StringId, DescriptorsWithLayout> m_PersistentDescriptors;
    std::unordered_map<ShaderNameWithOverrides, PipelineInfo, ShaderNameWithOverridesHasher> m_Pipelines;
    std::unordered_map<ShaderNameWithOverrides, ShaderPipelineTemplate, ShaderNameWithOverridesHasher>
        m_ShaderPipelineTemplates;
    std::unordered_map<StringId, PipelineInfo> m_DefaultPipelines;
    // todo: why is that a sting and not a std::filesystem::path?
    std::unordered_map<StringId, std::string> m_ShaderNameToPath;

    bakers::Context* m_BakersCtx{nullptr};
    const bakers::SlangBakeSettings* m_BakeSettings{nullptr};

    /* the fields below are used for hot-reloading */
    std::unordered_map<StringId, std::vector<ShaderNameWithOverrides>> m_ShaderNameToAllOverrides;
    StringUnorderedMap<std::vector<StringId>> m_PathToShaders;
    std::unique_ptr<FileWatcher> m_FileWatcher;
    FileWatcherHandler m_FileWatcherHandler;
    std::mutex m_FileUpdateMutex;
    std::vector<std::filesystem::path> m_ShadersToReload;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};