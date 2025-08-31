#pragma once

#include "FrameContext.h"
#include "ShaderPipelineTemplate.h"
#include "ShaderOverrides.h"
#include "Platform/FileWatcher.h"
#include "String/StringHeterogeneousHasher.h"
#include "String/StringId.h"

#include <mutex>

class Shader
{
    friend class ShaderCache;
public:
    Pipeline Pipeline() const { return m_Pipeline; }
    PipelineLayout GetLayout() const { return m_PipelineLayout; }
    const ::Descriptors& Descriptors(DescriptorsKind kind) const { return m_Descriptors[(u32)kind]; }
    const ::DescriptorsLayout& DescriptorsLayout(DescriptorsKind kind) const { return m_DescriptorLayouts[(u32)kind]; }
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
enum class ShaderCacheAllocationHint : u8
{
    Pipeline    = BIT(0),
    Descriptors = BIT(1),

    Complete = Pipeline | Descriptors,
};
CREATE_ENUM_FLAGS_OPERATORS(ShaderCacheAllocationHint)

using ShaderCacheAllocateResult = std::expected<Shader, ShaderCacheError>;

class ShaderCache
{
    static constexpr std::string_view SHADER_EXTENSION = ".shader";
    static constexpr std::string_view SHADER_HEADER_EXTENSION = ".glsl";
public:
    void Init();
    void Shutdown();
    void OnFrameBegin(FrameContext& ctx);
    
    ShaderCacheAllocateResult Allocate(StringId name, DescriptorArenaAllocators& allocators,
        ShaderCacheAllocationHint allocationHint = ShaderCacheAllocationHint::Complete);
    ShaderCacheAllocateResult Allocate(StringId name, ShaderOverridesView&& overrides,
        DescriptorArenaAllocators& allocators,
        ShaderCacheAllocationHint allocationHint = ShaderCacheAllocationHint::Complete);
    void AddPersistentDescriptors(StringId name, Descriptors descriptors, DescriptorsLayout descriptorsLayout);
private:
    void InitFileWatcher();
    void LoadShaderInfos();

    void HandleModifications();
    void HandleShaderModification(const std::filesystem::path& path);
    void HandleStageModification(const std::filesystem::path& path);
    void HandleHeaderModification(const std::filesystem::path& path);
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
        u64 OverridesHash{};

        auto operator<=>(const ShaderNameWithOverrides&) const = default;
    };
    std::optional<PipelineInfo> TryCreatePipeline(const ShaderNameWithOverrides& name, ShaderOverridesView& overrides,
        ShaderCacheAllocationHint allocationHint);
    const ShaderPipelineTemplate* GetShaderPipelineTemplate(const ShaderNameWithOverrides& name,
        const ShaderOverridesView& overrides, std::vector<std::string>& stages,
        const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides);
private:
    struct ShaderNameWithOverridesHasher
    {
        constexpr u64 operator()(ShaderNameWithOverrides shader) const
        {
            u64 hash = shader.Name.Hash();
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
    std::unordered_map<StringId, std::string> m_ShaderNameToPath;

    /* the fields below are used for hot-reloading */
    std::unordered_map<StringId, std::vector<ShaderNameWithOverrides>> m_ShaderNameToAllOverrides;
    StringUnorderedMap<std::vector<StringId>> m_PathToShaders;
    FileWatcher m_FileWatcher;
    FileWatcherHandler m_FileWatcherHandler;
    std::mutex m_FileUpdateMutex;
    std::vector<std::filesystem::path> m_ShadersToReload;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};