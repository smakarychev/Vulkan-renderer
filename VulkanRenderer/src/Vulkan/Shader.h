#pragma once
#include <array>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "AssetLib.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
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

// todo: WIP, reflection testing
// todo: name is pretty bad
class Shader
{
public:
    struct ReflectionData
    {
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
            };
            u32 Set;
            VkDescriptorSetLayoutCreateFlags LayoutFlags;
            VkDescriptorPoolCreateFlags PoolFlags;
            std::vector<Descriptor> Descriptors;
            std::vector<u32> VariableDescriptorCounts;
        };

        VkShaderStageFlags ShaderStages;
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
    void ReflectFrom(const std::vector<std::string_view>& paths);
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
    
    Pipeline::Builder m_PipelineBuilder{};
    PipelineLayout m_PipelineLayout;
    std::vector<DescriptorSetLayout*> m_DescriptorSetLayouts;

    std::vector<ShaderModuleData> m_Shaders;
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
            const RenderPass* RenderPass;
            std::array<DescriptorSet::Builder, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorBuilders;
        };
        using DescriptorInfo = ShaderPipelineTemplate::DescriptorInfo;
    public:
        ShaderPipeline Build();
        Builder& PrimitiveKind(PrimitiveKind primitiveKind);
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& CompatibleWithVertex(const VertexInputDescription& vertexInputDescription);
    private:
        void Prebuild();
        void CreateCompatibleLayout();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_CompatibleVertexDescription;
        ::PrimitiveKind m_PrimitiveKind{PrimitiveKind::Triangle};
    };
public:
    static ShaderPipeline Create(const Builder::CreateInfo& createInfo);

    void Bind(const CommandBuffer& cmd, VkPipelineBindPoint bindPoint);
    
    const Pipeline& GetPipeline() const { return m_Pipeline; }
    const PipelineLayout& GetPipelineLayout() const { return m_Template->GetPipelineLayout(); }

    bool operator==(const ShaderPipeline& other) const { return m_Pipeline == other.m_Pipeline; }
    bool operator!=(const ShaderPipeline& other) const { return !(*this == other); }
private:
    ShaderPipelineTemplate* m_Template{nullptr};

    Pipeline m_Pipeline;
};

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
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& AddBinding(std::string_view name, const Buffer& buffer);
        Builder& AddBinding(std::string_view name, const Buffer& buffer, u64 sizeBytes, u64 offset);
        Builder& AddBinding(std::string_view name, const Texture& texture);
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

    void Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout,
        VkPipelineBindPoint bindPoint);
    void Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind, const PipelineLayout& pipelineLayout,
        VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets);

    void SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex);
    
    const DescriptorSetsInfo& GetDescriptorSetsInfo() const { return m_DescriptorSetsInfo; }
    const DescriptorSet& GetDescriptorSet(DescriptorKind kind) const { return m_DescriptorSetsInfo.DescriptorSets[(u32)kind].Set; }
private:
    ShaderPipelineTemplate* m_Template{nullptr};
    
    DescriptorSetsInfo m_DescriptorSetsInfo{};
};