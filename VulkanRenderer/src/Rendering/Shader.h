#pragma once

#include <array>
#include <vector>

#include "Descriptors.h"
#include "DescriptorsTraits.h"
#include "Pipeline.h"
#include "ShaderAsset.h"
#include "types.h"

class DescriptorSet;
struct ShaderPushConstantDescription;
class DescriptorsLayout;
class DescriptorLayoutCache;
class DescriptorAllocator;
class Image;

// todo: this is probably deprecated
static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 3;
static_assert(MAX_PIPELINE_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");
enum class DescriptorKind : u32
{
    Global = 0,
    Pass = 1,
    Material = 2
};

struct ShaderPushConstantDescription
{
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage StageFlags{};
};

struct ShaderModuleSource
{
    std::vector<u8> Source;
    ShaderStage Stage;
};

class Shader
{
public:
    struct ReflectionData
    {
        using SpecializationConstant = assetLib::ShaderStageInfo::SpecializationConstant;
        using InputAttribute = assetLib::ShaderStageInfo::InputAttribute;
        using PushConstant = assetLib::ShaderStageInfo::PushConstant;
        struct DescriptorSet
        {
            struct DescriptorSetBindingNamedFlagged
            {
                std::string Name;
                DescriptorBinding Descriptor;
            };
            u32 Set; 
            bool HasBindless{false};
            bool HasImmutableSampler{false};
            std::vector<DescriptorSetBindingNamedFlagged> Descriptors;
        };

        ShaderStage ShaderStages;
        std::vector<SpecializationConstant> SpecializationConstants;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;

        /* the file dependencies of a shader */
        std::vector<std::string> Dependencies;
    };
public:
    static Shader* ReflectFrom(const std::vector<std::string_view>& paths);
    const ReflectionData& GetReflectionData() const { return m_ReflectionData; }
    const std::vector<ShaderModuleSource>& GetShadersSource() const { return m_Modules; }
private:
    assetLib::ShaderStageInfo LoadFromAsset(std::string_view path);
    static assetLib::ShaderStageInfo MergeReflections(const assetLib::ShaderStageInfo& first, const assetLib::ShaderStageInfo& second);
    static std::vector<ReflectionData::DescriptorSet> ProcessDescriptorSets(
        const std::vector<assetLib::ShaderStageInfo::DescriptorSet>& sets);
private:
    std::vector<ShaderModuleSource> m_Modules;
    ReflectionData m_ReflectionData{};
};

class ShaderPipelineTemplate
{
    friend class ShaderPipeline;
    friend class ShaderDescriptorSet;
    friend class ShaderDescriptors;
private:
    struct DescriptorsFlags
    {
        std::vector<DescriptorBinding> Descriptors;
        std::vector<DescriptorFlags> Flags;
    };
public:
    using ReflectionData = Shader::ReflectionData;
    class Builder
    {
        friend class ShaderPipelineTemplate;
        struct CreateInfo
        {
            Shader* ShaderReflection;
            DescriptorAllocator* Allocator{nullptr};
            DescriptorArenaAllocator* ResourceAllocator{nullptr};
            DescriptorArenaAllocator* SamplerAllocator{nullptr};
        };
    public:
        ShaderPipelineTemplate Build();
        Builder& SetShaderReflection(Shader* shaderReflection);
        Builder& SetDescriptorAllocator(DescriptorAllocator* allocator);
        Builder& SetDescriptorArenaResourceAllocator(DescriptorArenaAllocator* allocator);
        Builder& SetDescriptorArenaSamplerAllocator(DescriptorArenaAllocator* allocator);
    private:
        CreateInfo m_CreateInfo;
    };

    struct DescriptorSetInfo
    {
        std::vector<std::string> Names;
        std::vector<DescriptorBinding> Bindings;
    };

    using SpecializationConstant = Shader::ReflectionData::SpecializationConstant;
    
public:
    static ShaderPipelineTemplate Create(const Builder::CreateInfo& createInfo);

    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    const DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    const DescriptorBinding& GetBinding(u32 set, std::string_view name) const;
    const DescriptorBinding* TryGetBinding(u32 set, std::string_view name) const;
    std::pair<u32, const DescriptorBinding&> GetSetAndBinding(std::string_view name) const;
    std::array<bool, MAX_PIPELINE_DESCRIPTOR_SETS> GetSetPresence() const;
    
    bool IsComputeTemplate() const { return m_IsComputeTemplate; }

    const std::vector<std::string>& GetShaderDependencies() const { return m_ShaderDependencies; }
    
private:
    static std::vector<DescriptorsLayout> CreateDescriptorLayouts(
        const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections, bool useDescriptorBuffer);
    static VertexInputDescription CreateInputDescription(
        const std::vector<ReflectionData::InputAttribute>& inputAttributeReflections);
    static std::vector<ShaderPushConstantDescription> CreatePushConstantDescriptions(
        const std::vector<ReflectionData::PushConstant>& pushConstantReflections);
    static DescriptorsFlags ExtractDescriptorsAndFlags(const ReflectionData::DescriptorSet& descriptorSet,
        bool useDescriptorBuffer);
private:
    struct Allocator
    {
        DescriptorAllocator* DescriptorAllocator{nullptr};
        DescriptorArenaAllocator* ResourceAllocator{nullptr};    
        DescriptorArenaAllocator* SamplerAllocator{nullptr};       
    };
    Allocator m_Allocator{};
    bool m_UseDescriptorBuffer{false};

    VertexInputDescription m_VertexInputDescription;
    Pipeline::Builder m_PipelineBuilder{};
    PipelineLayout m_PipelineLayout;
    std::vector<DescriptorsLayout> m_DescriptorsLayouts;

    std::vector<SpecializationConstant> m_SpecializationConstants;
    std::array<DescriptorSetInfo, MAX_PIPELINE_DESCRIPTOR_SETS> m_DescriptorSetsInfo;
    std::vector<DescriptorPoolFlags> m_DescriptorPoolFlags;
    u32 m_DescriptorSetCount;

    bool m_IsComputeTemplate{false};

    std::vector<std::string> m_ShaderDependencies;
};

class ShaderPipeline
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class ShaderPipeline;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            ShaderPipelineTemplate* ShaderPipelineTemplate;
            RenderingDetails RenderingDetails;
            std::array<DescriptorSet::Builder, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorBuilders;
            PipelineSpecializationInfo PipelineSpecializationInfo;
            bool UseDescriptorBuffer{false};
        };
    public:
        ShaderPipeline Build();
        ShaderPipeline Build(DeletionQueue& deletionQueue);
        ShaderPipeline BuildManualLifetime();
        Builder& SetRenderingDetails(const RenderingDetails& renderingDetails);
        Builder& DynamicStates(DynamicStates states);
        Builder& DepthClamp(bool enable = true);
        Builder& DepthMode(DepthMode depthMode);
        Builder& FaceCullMode(FaceCullMode cullMode);
        Builder& PrimitiveKind(PrimitiveKind primitiveKind);
        Builder& AlphaBlending(AlphaBlending alphaBlending);
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& CompatibleWithVertex(const VertexInputDescription& vertexInputDescription);
        template <typename T>
        Builder& AddSpecialization(std::string_view name, const T& specializationData);
        Builder& UseDescriptorBuffer();
    private:
        void Prebuild();
        void CreateCompatibleLayout();
        void FinishSpecializationConstants();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_CompatibleVertexDescription;
        ::DynamicStates m_DynamicStates{DynamicStates::Default};
        bool m_ClampDepth{false};
        ::DepthMode m_DepthMode{DepthMode::ReadWrite};
        ::FaceCullMode m_CullMode{FaceCullMode::None};
        ::PrimitiveKind m_PrimitiveKind{PrimitiveKind::Triangle};
        ::AlphaBlending m_AlphaBlending{AlphaBlending::Over};
        PipelineSpecializationInfo m_PipelineSpecializationInfo;
        std::vector<std::string> m_SpecializationConstantNames;
    };
public:
    static ShaderPipeline Create(const Builder::CreateInfo& createInfo);

    void BindGraphics(const CommandBuffer& cmd) const;
    void BindCompute(const CommandBuffer& cmd) const;
    
    Pipeline GetPipeline() const { return m_Pipeline; }
    PipelineLayout GetLayout() const { return m_Template->GetPipelineLayout(); }

    const ShaderPipelineTemplate* GetTemplate() const { return m_Template; }
    ShaderPipelineTemplate* GetTemplate() { return m_Template; }

    bool operator==(const ShaderPipeline& other) const { return m_Pipeline == other.m_Pipeline; }
    bool operator!=(const ShaderPipeline& other) const { return !(*this == other); }
private:
    ShaderPipelineTemplate* m_Template{nullptr};

    Pipeline m_Pipeline;
};

template <typename T>
ShaderPipeline::Builder& ShaderPipeline::Builder::AddSpecialization(std::string_view name, const T& specializationData)
{
    u32 dataSizeBytes;
    // vulkan spec says that bool specialization constant must have the size of VkBool32
    if constexpr (std::is_same_v<T, bool>)
        dataSizeBytes = sizeof(u32);
    else
        dataSizeBytes = sizeof(specializationData);
    
    // template might not be set yet, so we have to delay it until the very end
    m_SpecializationConstantNames.emplace_back(name);
    u32 bufferStart = (u32)m_PipelineSpecializationInfo.Buffer.size();
    m_PipelineSpecializationInfo.Buffer.resize(bufferStart + dataSizeBytes);
    if constexpr (std::is_same_v<T, bool>)
    {
        u32 data = (u32)specializationData;
        std::memcpy(m_PipelineSpecializationInfo.Buffer.data() + bufferStart, &data, dataSizeBytes);
    }
    else
    {
        std::memcpy(m_PipelineSpecializationInfo.Buffer.data() + bufferStart, &specializationData, dataSizeBytes);
    }
    
    m_PipelineSpecializationInfo.ShaderSpecializations.push_back({
        .SizeBytes = dataSizeBytes,
        .Offset = bufferStart});

    return *this;
}

class ShaderDescriptorSet
{
    using Texture = Image;
public:
    class Builder
    {
        friend class ShaderDescriptorSet;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            ShaderPipelineTemplate* ShaderPipelineTemplate;
            std::array<DescriptorSet::Builder, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorBuilders;
            std::array<bool, MAX_PIPELINE_DESCRIPTOR_SETS> SetPresence{};
        };
    public:
        ShaderDescriptorSet Build();
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& AddBinding(std::string_view name, const Buffer& buffer);
        Builder& AddBinding(std::string_view name, const Buffer& buffer, u64 sizeBytes, u64 offset);
        Builder& AddBinding(std::string_view name, const DescriptorSet::TextureBindingInfo& texture);
        Builder& AddBinding(std::string_view name, u32 variableBindingCount);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
    };

    struct DescriptorSetsInfo
    {
        struct SetInfo
        {
            bool IsPresent{false};
            DescriptorSet Set; 
        };
        std::array<SetInfo, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorSets;
        u32 DescriptorCount{0};
    };
public:
    static ShaderDescriptorSet Create(const Builder::CreateInfo& createInfo);
    
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout)
        const;
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout,
        const std::vector<u32>& dynamicOffsets) const;

    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout)
        const;
    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout,
                     const std::vector<u32>& dynamicOffsets) const;

    void SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex);
    
    const DescriptorSetsInfo& GetDescriptorSetsInfo() const { return m_DescriptorSetsInfo; }
    const DescriptorSet& GetDescriptorSet(DescriptorKind kind) const
    {
        return m_DescriptorSetsInfo.DescriptorSets[(u32)kind].Set;
    }
private:
    ShaderPipelineTemplate* m_Template{nullptr};
    
    DescriptorSetsInfo m_DescriptorSetsInfo{};
};

enum class ShaderDescriptorsKind
{
    // todo: names here (e.g. 'Samplers', 'Global', 'Pass', 'Material')
};

namespace Experimental
{
    class ShaderCache;
}

class ShaderDescriptors
{
    friend class Experimental::ShaderCache;
public:
    using Texture = Image;
    class Builder
    {
        friend class ShaderDescriptors;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            const ShaderPipelineTemplate* ShaderPipelineTemplate{nullptr};
            DescriptorArenaAllocator* Allocator{nullptr};
            u32 Set{0};
            u32 BindlessCount{0};
        };
    public:
        ShaderDescriptors Build();
        Builder& SetTemplate(const ShaderPipelineTemplate* shaderPipelineTemplate,
            DescriptorAllocatorKind allocatorKind);
        Builder& ExtractSet(u32 set);
        Builder& BindlessCount(u32 count);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    using BindingInfo = Descriptors::BindingInfo;
    
    static ShaderDescriptors Create(const Builder::CreateInfo& createInfo);

    void BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout) const;
    void BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout) const;
    void BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const;
    void BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const;

    void UpdateBinding(std::string_view name, const BufferBindingInfo& buffer) const;
    void UpdateBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateBinding(std::string_view name, const TextureBindingInfo& texture) const;
    void UpdateBinding(std::string_view name, const TextureBindingInfo& texture, u32 index) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture, u32 index) const;

    void UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer) const;
    void UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture) const;
    void UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture, u32 index) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
        u32 index) const;

    BindingInfo GetBindingInfo(std::string_view bindingName) const;
    
private:
    Descriptors m_Descriptors{};
    u32 m_SetNumber{0};
    
    const ShaderPipelineTemplate* m_Template{nullptr};
};

class ShaderTemplateLibrary
{
public:
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorAllocator& allocator);
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(const std::string& name, DescriptorArenaAllocators& allocators);
    
    static ShaderPipelineTemplate* CreateMaterialsTemplate(const std::string& templateName,
        DescriptorArenaAllocators& allocators);

    static ShaderPipelineTemplate* ReloadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(const std::string& name);
    static std::string GenerateTemplateName(std::string_view templateName, DescriptorAllocator& allocator);
    static std::string GenerateTemplateName(std::string_view templateName, DescriptorArenaAllocators& allocators);
    static void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name);
private:
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string_view>& paths,
        DescriptorAllocator& allocator);
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string_view>& paths,
        DescriptorArenaAllocators& allocators);
private:
    static std::unordered_map<std::string, ShaderPipelineTemplate> s_Templates;
};