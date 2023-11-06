#pragma once

#include "types.h"

#include "VulkanCommon.h"

#include <vector>
#include <vulkan/vulkan_core.h>

class DescriptorSetLayout;
class PushConstantDescription;
class CommandBuffer;

class PipelineLayout
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class PipelineLayout;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<VkPushConstantRange> PushConstantRanges;
            std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
        };
    public:
        PipelineLayout Build();
        Builder& SetPushConstants(const std::vector<PushConstantDescription>& pushConstants);
        Builder& SetDescriptorLayouts(const std::vector<DescriptorSetLayout*>& layouts);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static PipelineLayout Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const PipelineLayout& pipelineLayout);
private:
    VkPipelineLayout m_Layout{VK_NULL_HANDLE};
};

class Pipeline
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Pipeline;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            bool IsComputePipeline{false};
            RenderingDetails RenderingDetails;
            std::vector<VkPipelineShaderStageCreateInfo> Shaders;
            std::vector<VkDynamicState> DynamicStates;
            VkPipelineDynamicStateCreateInfo DynamicStateInfo;
            VkPipelineViewportStateCreateInfo ViewportState;
            VkPipelineInputAssemblyStateCreateInfo InputAssemblyState;
            VkPipelineVertexInputStateCreateInfo VertexInputState;
            VkPipelineRasterizationStateCreateInfo RasterizationState;
            VkPipelineMultisampleStateCreateInfo MultisampleState;
            VkPipelineDepthStencilStateCreateInfo DepthStencilState;
            VkPipelineColorBlendAttachmentState ColorBlendAttachmentState;
            VkPipelineColorBlendStateCreateInfo ColorBlendState;
            VkPipelineLayout Layout;
        };
    public:
        Pipeline Build();
        Pipeline BuildManualLifetime();
        Builder& SetLayout(const PipelineLayout& layout);
        Builder& SetRenderingDetails(const RenderingDetails& renderingDetails);
        Builder& IsComputePipeline(bool isCompute);
        Builder& AddShader(const ShaderModuleData& shaderModuleData);
        Builder& FixedFunctionDefaults();
        Builder& SetVertexDescription(const VertexInputDescription& vertexDescription);
        Builder& PrimitiveKind(PrimitiveKind primitiveKind);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_VertexInputDescription;
        ::PrimitiveKind m_PrimitiveKind{PrimitiveKind::Triangle};
    };
public:
    static Pipeline Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    void Bind(const CommandBuffer& commandBuffer, VkPipelineBindPoint bindPoint);

    bool operator==(const Pipeline& other) const { return m_Pipeline == other.m_Pipeline; }
    bool operator!=(const Pipeline& other) const { return !(*this == other); }
private:
    static Pipeline CreateGraphicsPipeline(const Builder::CreateInfo& createInfo);
    static Pipeline CreateComputePipeline(const Builder::CreateInfo& createInfo);
private:
    VkPipeline m_Pipeline{VK_NULL_HANDLE};
};
