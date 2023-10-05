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
    using ShaderReflection = assetLib::ShaderInfo;
    struct ShaderModule
    {
        std::vector<u8> Source;
        ShaderKind Kind;
    };
public:
    void LoadFromAsset(std::string_view path);
    void ReflectFrom(const std::vector<std::string_view>& paths);
    const assetLib::ShaderInfo& GetReflectionData() const { return m_ReflectionData; }
    const std::vector<ShaderModule>& GetShaders() const { return m_Modules; }
private:
    static assetLib::ShaderInfo MergeReflections(const assetLib::ShaderInfo& first, const assetLib::ShaderInfo& second); 
private:
    std::vector<ShaderModule> m_Modules;
    assetLib::ShaderInfo m_ReflectionData{};
};

class ShaderPipelineTemplate
{
    friend class ShaderPipeline;
    friend class ShaderDescriptorSet;
public:
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

    struct DescriptorsInfo
    {
        std::string Name;
        u32 Set;
        u32 Binding;
        VkDescriptorType Descriptor;
        VkShaderStageFlags ShaderStages;
    };
    
public:
    static ShaderPipelineTemplate Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate);

    PipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }
    const PipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
    
private:
    static std::vector<DescriptorSetLayout*> CreateDescriptorLayouts(const std::vector<Shader::ShaderReflection::DescriptorSet>& descriptorSetReflections, DescriptorLayoutCache* layoutCache);
    static VertexInputDescription CreateInputDescription(const std::vector<Shader::ShaderReflection::InputAttribute>& inputAttributeReflections);
    static std::vector<PushConstantDescription> CreatePushConstantDescriptions(const std::vector<Shader::ShaderReflection::PushConstant>& pushConstantReflections);
    static std::vector<ShaderModuleData> CreateShaderModules(const std::vector<Shader::ShaderModule>& shaders);
    static std::vector<VkDescriptorSetLayoutBinding> ExtractBindings(const Shader::ShaderReflection::DescriptorSet& descriptorSet);
private:
    DescriptorAllocator* m_Allocator{nullptr};
    DescriptorLayoutCache* m_LayoutCache{nullptr};

    VertexInputDescription m_VertexInputDescription;
    
    Pipeline::Builder m_PipelineBuilder{};
    PipelineLayout m_PipelineLayout;

    std::vector<ShaderModuleData> m_Shaders;
    std::vector<DescriptorsInfo> m_DescriptorsInfo;
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
        using BindingInfo = ShaderPipelineTemplate::DescriptorsInfo;
    public:
        ShaderPipeline Build();
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& CompatibleWithVertex(const VertexInputDescription& vertexInputDescription);
    private:
        void Prebuild();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_CompatibleVertexDescription;
    };
public:
    static ShaderPipeline Create(const Builder::CreateInfo& createInfo);

    void Bind(const CommandBuffer& cmd, VkPipelineBindPoint bindPoint);
    
    const Pipeline& GetPipeline() const { return m_Pipeline; }
    const PipelineLayout& GetPipelineLayout() const { return m_Template->GetPipelineLayout(); }
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
        using BindingInfo = ShaderPipelineTemplate::DescriptorsInfo;
    public:
        ShaderDescriptorSet Build();
        Builder& SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate);
        Builder& AddBinding(std::string_view name, const Buffer& buffer);
        Builder& AddBinding(std::string_view name, const Buffer& buffer, u64 sizeBytes, u64 offset);
        Builder& AddBinding(std::string_view name, const Texture& texture);
    private:
        void PreBuild();
        const BindingInfo& FindDescriptorSet(std::string_view name) const;
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

    void Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind, const ShaderPipeline& pipeline,
        VkPipelineBindPoint bindPoint);
    void Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind, const ShaderPipeline& pipeline,
        VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets);
    
    const DescriptorSetsInfo& GetDescriptorSetsInfo() const { return m_DescriptorSetsInfo; }
    const DescriptorSet& GetDescriptorSet(DescriptorKind kind) const { return m_DescriptorSetsInfo.DescriptorSets[(u32)kind].Set; }
private:
    DescriptorSetsInfo m_DescriptorSetsInfo{};
};