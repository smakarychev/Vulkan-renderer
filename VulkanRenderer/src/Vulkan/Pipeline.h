#pragma once

#include "types.h"

#include "VulkanCommon.h"

#include <vector>
#include <vulkan/vulkan_core.h>

class DescriptorSetLayout;
class PushConstantDescription;
class CommandBuffer;
class RenderPass;

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
            VkRenderPass RenderPass{VK_NULL_HANDLE};
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
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& AddShader(const ShaderModuleData& shaderModuleData);
        Builder& FixedFunctionDefaults();
        Builder& SetVertexDescription(const VertexInputDescription& vertexDescription);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
        VertexInputDescription m_VertexInputDescription;
    };
public:
    static Pipeline Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    void Bind(const CommandBuffer& commandBuffer, VkPipelineBindPoint bindPoint);
private:
    VkPipeline m_Pipeline{VK_NULL_HANDLE};
};
