#pragma once
#include <array>
#include <vector>
#include <Vulkan/vulkan_core.h>

#include "DescriptorSet.h"
#include "Pipeline.h"
#include "PushConstantDescription.h"
#include "ShaderAsset.h"
#include "types.h"
#include "VulkanCommon.h"

class DescriptorSet;
class PushConstantDescription;
class DescriptorSetLayout;
class DescriptorLayoutCache;
class DescriptorAllocator;
class Image;

static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 3;
static_assert(MAX_PIPELINE_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");
enum class DescriptorKind : u32
{
    Global = 0,
    Pass = 1,
    Material = 2
};

enum class ShaderKind
{
    Vertex, Pixel, Compute
};

struct ShaderModuleData
{
    // TODO: FIXME: direct VKAPI usage
    VkShaderModule Module;
    ShaderKind Kind;
};

class Shader
{
public:
    struct ReflectionData
    {
        using SpecializationConstant = assetLib::ShaderInfo::SpecializationConstant;
        using InputAttribute = assetLib::ShaderInfo::InputAttribute;
        using PushConstant = assetLib::ShaderInfo::PushConstant;
        struct DescriptorSet
        {
            struct Descriptor
            {
                u32 Binding;
                std::string Name;
                VkDescriptorType Type;
                u32 Count;
                VkShaderStageFlags ShaderStages;
                VkDescriptorBindingFlags Flags;
                bool IsImmutableSampler;
            };
            u32 Set;
            VkDescriptorSetLayoutCreateFlags LayoutFlags;
            VkDescriptorPoolCreateFlags PoolFlags;
            std::vector<Descriptor> Descriptors;
            std::vector<u32> VariableDescriptorCounts;
        };

        VkShaderStageFlags ShaderStages;
        std::vector<SpecializationConstant> SpecializationConstants;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;
    };
    struct ShaderModule
    {
        std::vector<u8> Source;
        ShaderKind Kind;
    };
public:
    static Shader* ReflectFrom(const std::vector<std::string_view>& paths);
    const ReflectionData& GetReflectionData() const { return m_ReflectionData; }
    const std::vector<ShaderModule>& GetShaders() const { return m_Modules; }
private:
    assetLib::ShaderInfo LoadFromAsset(std::string_view path);
    static assetLib::ShaderInfo MergeReflections(const assetLib::ShaderInfo& first, const assetLib::ShaderInfo& second);
    static std::vector<ReflectionData::DescriptorSet> ProcessDescriptorSets(const std::vector<assetLib::ShaderInfo::DescriptorSet>& sets);
    static u32 GetBindlessDescriptorCount(VkDescriptorType type);
private:
    std::vector<ShaderModule> m_Modules;
    ReflectionData m_ReflectionData{};
};

class ShaderPipelineTemplate
{
    friend class ShaderPipeline;
    friend class ShaderDescriptorSet;
public:
    using ReflectionData = Shader::ReflectionData;
    class Builder
    {
        friend class ShaderPipelineTemplate;
        struct CreateInfo
        {
            Shader* ShaderReflection;
            DescriptorAllocator* Allocator;
            DescriptorLayoutCache* LayoutCache;
        };
    public:
        ShaderPipelineTemplate Build();
        ShaderPipelineTemplate BuildManualLifetime();
        Builder& SetShaderReflection(Shader* shaderReflection);
        Builder& SetDescriptorAllocator(DescriptorAllocator* allocator);
        Builder& SetDescriptorLayoutCache(DescriptorLayoutCache* layoutCache);
    private:
        CreateInfo m_CreateInfo;
    };

    struct DescriptorInfo
    {
        std::string Name;
        u32 Set;
        u32 Binding;
        VkDescriptorType Type;
        VkShaderStageFlags ShaderStages;
        VkDescriptorBindingFlags Flags;
    };

    struct DescriptorsFlags
    {
        std::vector<VkDescriptorSetLayoutBinding> Descriptors;
        std::vector<VkDescriptorBindingFlags> Flags;
    };

    using SpecializationConstant = Shader::ReflectionData::SpecializationConstant;
    
public:
    static ShaderPipelineTemplate Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate);

    PipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }
    const PipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
    const DescriptorSetLayout* GetDescriptorSetLayout(u32 index) const { return m_DescriptorSetLayouts[index]; }

    const DescriptorInfo& GetDescriptorInfo(std::string_view name);
    
    bool IsComputeTemplate() const;
    
private:
    static std::vector<DescriptorSetLayout*> CreateDescriptorLayouts(const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections, DescriptorLayoutCache* layoutCache);
    static VertexInputDescription CreateInputDescription(const std::vector<ReflectionData::InputAttribute>& inputAttributeReflections);
    static std::vector<PushConstantDescription> CreatePushConstantDescriptions(const std::vector<ReflectionData::PushConstant>& pushConstantReflections);
    static std::vector<ShaderModuleData> CreateShaderModules(const std::vector<Shader::ShaderModule>& shaders);
    static DescriptorsFlags ExtractDescriptorsAndFlags(const ReflectionData::DescriptorSet& descriptorSet);
private:
    DescriptorAllocator* m_Allocator{nullptr};
    DescriptorLayoutCache* m_LayoutCache{nullptr};

    VertexInputDescription m_VertexInputDescription;
    PushConstantDescription m_PushConstantDescription;
    
    Pipeline::Builder m_PipelineBuilder{};
    PipelineLayout m_PipelineLayout;
    std::vector<DescriptorSetLayout*> m_DescriptorSetLayouts;

    std::vector<ShaderModuleData> m_Shaders;
    std::vector<SpecializationConstant> m_SpecializationConstants;
    std::vector<DescriptorInfo> m_DescriptorsInfo;
    std::vector<VkDescriptorSetLayoutCreateFlags> m_DescriptorSetFlags;
    std::vector<VkDescriptorPoolCreateFlags> m_DescriptorPoolFlags;
    u32 m_DescriptorSetCount;
};

class ShaderPipeline
{
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
        };
        using DescriptorInfo = ShaderPipelineTemplate::DescriptorInfo;
    public:
        ShaderPipeline Build();
        Builder& SetRenderingDetails(const RenderingDetails& renderingDetails);
        Builder& PrimitiveKind(PrimitiveKind primitiveKind);
        Builder& AlphaBlending(AlphaBlending alphaBlending);
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& CompatibleWithVertex(const VertexInputDescription& vertexInputDescription);
        template <typename T>
        Builder& AddSpecialization(std::string_view name, const T& specializationData);
    private:
        void Prebuild();
        void CreateCompatibleLayout();
        void FinishSpecializationConstants();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_CompatibleVertexDescription;
        ::PrimitiveKind m_PrimitiveKind{PrimitiveKind::Triangle};
        ::AlphaBlending m_AlphaBlending{AlphaBlending::Over};
        PipelineSpecializationInfo m_PipelineSpecializationInfo;
        std::vector<std::string> m_SpecializationConstantNames;
    };
public:
    static ShaderPipeline Create(const Builder::CreateInfo& createInfo);

    void BindGraphics(const CommandBuffer& cmd) const;
    void BindCompute(const CommandBuffer& cmd) const;
    
    const Pipeline& GetPipeline() const { return m_Pipeline; }
    const PipelineLayout& GetPipelineLayout() const { return m_Template->GetPipelineLayout(); }
    const PushConstantDescription& GetPushConstantDescription() const { return m_Template->m_PushConstantDescription; }

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
    m_SpecializationConstantNames.push_back(std::string{name});
    u32 bufferStart = (u32)m_PipelineSpecializationInfo.Buffer.size();
    m_PipelineSpecializationInfo.Buffer.resize(bufferStart + dataSizeBytes);
    std::memcpy(m_PipelineSpecializationInfo.Buffer.data() + bufferStart, &specializationData, dataSizeBytes);
    m_PipelineSpecializationInfo.ShaderSpecializations.push_back({
        .SpecializationEntry = {
            .offset = bufferStart,
            .size = dataSizeBytes}});

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
            std::array<u32, MAX_PIPELINE_DESCRIPTOR_SETS> UsedSets{0};
        };
        using DescriptorInfo = ShaderPipelineTemplate::DescriptorInfo;
    public:
        ShaderDescriptorSet Build();
        ShaderDescriptorSet BuildManualLifetime();
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
    static void Destroy(const ShaderDescriptorSet& descriptorSet);
    
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout)
        const;
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout,
        const std::vector<u32>& dynamicOffsets) const;

    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout)
        const;
    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout,
        const std::vector<u32>& dynamicOffsets) const;

    void SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex);
    
    const DescriptorSetsInfo& GetDescriptorSetsInfo() const { return m_DescriptorSetsInfo; }
    const DescriptorSet& GetDescriptorSet(DescriptorKind kind) const { return m_DescriptorSetsInfo.DescriptorSets[(u32)kind].Set; }
private:
    ShaderPipelineTemplate* m_Template{nullptr};
    
    DescriptorSetsInfo m_DescriptorSetsInfo{};
};

class ShaderTemplateLibrary
{
public:
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName,
        DescriptorAllocator& allocator,
        DescriptorLayoutCache& layoutCache);
    static ShaderPipelineTemplate* GetShaderTemplate(const std::string& name);
private:
    static void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name);
private:
    static std::unordered_map<std::string, ShaderPipelineTemplate> m_Templates;
};