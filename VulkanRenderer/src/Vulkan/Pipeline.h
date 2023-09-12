#pragma once

#include "types.h"

#include "VulkanCommon.h"

#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

class DescriptorSetLayout;
class PushConstantDescription;
class CommandBuffer;
class RenderPass;

enum class ShaderKind
{
    Vertex, Pixel
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
            std::vector<VkShaderModule> ShaderModules;
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
            VkPipelineLayoutCreateInfo PipelineLayout;
            std::vector<VkPushConstantRange> PushConstantRanges;
            std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
        };
        struct ShaderModuleData
        {
            VkShaderModule Module;
            ShaderKind Kind;
        };
    public:
        Pipeline Build();
        Pipeline BuildManualLifetime();
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& AddShader(ShaderKind shaderKind, std::string_view shaderPath);
        Builder& FixedFunctionDefaults();
        Builder& SetVertexDescription(const VertexInputDescription& vertexDescription);
        Builder& AddPushConstant(const PushConstantDescription& description);
        Builder& AddDescriptorLayout(const DescriptorSetLayout& layout);
    private:
        void FinishShaders();
        void FinishFixedFunction();
        VkShaderModule CreateShader(const std::vector<u8>& spirv) const;
    private:
        CreateInfo m_CreateInfo;
        std::vector<ShaderModuleData> m_ShaderModules;
        VertexInputDescription m_VertexInputDescription;
    };
public:
    static Pipeline Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    void Bind(const CommandBuffer& commandBuffer, VkPipelineBindPoint bindPoint);
private:
    u32 FindDescriptorSetLayout(VkDescriptorSetLayout layout) const;
private:
    VkPipeline m_Pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_Layout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
};
