#pragma once
#include "ShaderAsset.h"
#include "Assets/AssetSystem.h"
#include "Rendering/Shader/ShaderOverrides.h"

struct FrameContext;

namespace lux
{
class ShaderAssetManager;

struct ShaderOverrides
{
    ShaderDynamicSpecializations Specializations{};
    ShaderPipelineOverrides PipelineStateOverride{};

    constexpr u64 Hash() const;
    auto operator<=>(const ShaderOverrides&) const = default;
    
    constexpr ShaderOverrides OverrideBy(const ShaderOverrides& other) const;
    constexpr ShaderOverrides OverrideBy(Span<const ShaderOverrides> overrides) const;
};

struct ShaderPipeline
{
    Pipeline Pipeline{};
    PipelineLayout PipelineLayout{};
    std::array<Descriptors, MAX_DESCRIPTOR_SETS> Descriptors;
};

enum class ShaderPipelineLibraryError : u8
{
    FailedToCreatePipeline,
    FailedToAllocateDescriptors,
};

struct ShaderTextureHeapAllocation
{
    Descriptors Descriptors{};
    PipelineLayout PipelineLayout{};
};

using ShaderPipelineLibraryAllocateResult = std::expected<ShaderPipeline, ShaderPipelineLibraryError>;
using ShaderPipelineLibraryTextureHeapResult = std::expected<ShaderTextureHeapAllocation, ShaderPipelineLibraryError>;

class ShaderPipelineLibrary
{
public:
    ShaderPipelineLibrary(ShaderAssetManager& shaderAssetManager);
    void Shutdown();
    void OnFrameBegin(FrameContext& ctx);
    
    ShaderPipelineLibraryAllocateResult Allocate(ShaderHandle shader, const ShaderOverrides& overrides, 
        DescriptorArenaAllocators& allocators);
    ShaderPipelineLibraryTextureHeapResult CreateTextureHeap(DescriptorArenaAllocator persistentAllocator, u32 count);
private:
    std::optional<Pipeline> FindOrCreatePipeline(ShaderHandle shaderHandle, const ShaderAsset& shader,
        const ShaderOverrides& overrides);
    Pipeline CreatePipeline(const ShaderAsset& shader, const ShaderOverrides& overrides);
    void OnShaderUpdated(ShaderHandle shader);
private:
    struct CashKey
    {
        ShaderHandle Shader{};
        ShaderOverrides Overrides{};
        
        auto operator<=>(const CashKey&) const = default;
    };
    struct CashKeyHasher
    {
        u64 operator()(const CashKey& key) const
        {
            u64 hash = std::hash<ShaderHandle>{}(key.Shader);
            Hash::combine(hash, key.Overrides.Hash());
            
            return hash;
        }
    };
    struct DescriptorsWithLayout
    {
        Descriptors Descriptors{};
        DescriptorsLayout Layout{};
    };
private:
    std::mutex m_Mutex;
    std::unordered_map<CashKey, Pipeline, CashKeyHasher> m_PipelineCache;
    ShaderAssetManager* m_ShaderAssetManager{nullptr};
    DescriptorsWithLayout m_TextureHeap{};
    
    AssetUpdatedHandler m_ShaderUpdatedHandler;
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};

constexpr u64 ShaderOverrides::Hash() const
{
    u64 hash = Specializations.Hash;
    Hash::combine(hash, PipelineStateOverride.Hash());
    
    return hash;
}

constexpr ShaderOverrides ShaderOverrides::OverrideBy(const ShaderOverrides& other) const
{
    return OverrideBy(Span<const ShaderOverrides>({other}));
}

constexpr ShaderOverrides ShaderOverrides::OverrideBy(Span<const ShaderOverrides> overrides) const
{
    ShaderOverrides merged = *this;

    bool recalculateSpecializationsHash = false;
    
    for (auto& other : overrides)
    {
        for (auto&& [i, spec] : std::views::enumerate(other.Specializations.Names))
        {
            recalculateSpecializationsHash = true;
            
            auto it = std::ranges::find_if(merged.Specializations.Names,
                [&](auto name){ return name == spec; });
            if (it == merged.Specializations.Names.end())
            {
                merged.Specializations.Names.emplace_back(spec);
                auto& description = other.Specializations.Descriptions[i];
                merged.Specializations.Descriptions.push_back({
                    .SizeBytes = description.SizeBytes,
                    .Offset = (u32)merged.Specializations.Data.size()});
                merged.Specializations.Data.append_range(
                    std::span(other.Specializations.Data.data() + description.Offset, description.SizeBytes));                
                continue;
            }

            u32 index = u32(it - merged.Specializations.Names.begin());
            auto& thisDesc = merged.Specializations.Descriptions[index];
            auto& otherDesc = other.Specializations.Descriptions[i];
            ASSERT(thisDesc.SizeBytes == otherDesc.SizeBytes, "Unable to merge")
            std::memcpy(
                merged.Specializations.Data.data() + thisDesc.Offset,
                other.Specializations.Data.data() + otherDesc.Offset,
                otherDesc.SizeBytes);
        }
        
        auto& pipelineState = merged.PipelineStateOverride;
        auto& otherState = other.PipelineStateOverride;

        pipelineState.DynamicStates = otherState.DynamicStates ?
            *otherState.DynamicStates : pipelineState.DynamicStates;
        pipelineState.DepthMode = otherState.DepthMode ? *otherState.DepthMode : pipelineState.DepthMode;
        pipelineState.DepthTest = otherState.DepthTest ? *otherState.DepthTest : pipelineState.DepthTest;
        pipelineState.CullMode = otherState.CullMode ? *otherState.CullMode : pipelineState.CullMode;
        pipelineState.AlphaBlending = otherState.AlphaBlending ?
            *otherState.AlphaBlending : pipelineState.AlphaBlending;
        pipelineState.PrimitiveKind = otherState.PrimitiveKind ?
            *otherState.PrimitiveKind : pipelineState.PrimitiveKind;
        pipelineState.ClampDepth = otherState.ClampDepth ? *otherState.ClampDepth : pipelineState.ClampDepth;
    }
    
    if (recalculateSpecializationsHash)
    {
        merged.Specializations.Hash = 0;
        for (u32 i = 0; i < merged.Specializations.Descriptions.size(); i++)
        {
            auto& desc = merged.Specializations.Descriptions[i];
            Hash::combine(
                merged.Specializations.Hash,
                merged.Specializations.Names[i].Hash() ^
                Hash::bytes(merged.Specializations.Data.data() + desc.Offset, desc.SizeBytes));
        }
    }

    return merged;
}
}
