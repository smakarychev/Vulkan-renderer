#pragma once

#include "types.h"

#include "VulkanCommon.h"

#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

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
            VkDevice Device{VK_NULL_HANDLE};
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
            VkPipelineColorBlendAttachmentState ColorBlendAttachmentState;
            VkPipelineColorBlendStateCreateInfo ColorBlendState;
            VkPipelineLayoutCreateInfo PipelineLayout;
        };
        struct ShaderModuleData
        {
            VkShaderModule Module;
            ShaderKind Kind;
        };
    public:
        Pipeline Build();
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& AddShader(ShaderKind shaderKind, std::string_view shaderPath);
        Builder& FixedFunctionDefaults();
    private:
        void FinishShaders();
        VkShaderModule CreateShader(const std::vector<u32>& spirv) const;
    private:
        CreateInfo m_CreateInfo;
        std::vector<ShaderModuleData> m_ShaderModules;
    };
public:
    static Pipeline Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    void Bind(const CommandBuffer& commandBuffer, VkPipelineBindPoint bindPoint);
private:
    VkPipeline m_Pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_Layout{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
};
