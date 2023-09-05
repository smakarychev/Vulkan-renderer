#include "Pipeline.h"

#include <shaderc/shaderc.h>

#include "core.h"
#include "Driver.h"
#include "RenderCommand.h"
#include "utils.h"

namespace
{
    shaderc_shader_kind kindToShaderCKind(ShaderKind kind)
    {
        switch (kind)
        {
        case ShaderKind::Vertex:
            return shaderc_vertex_shader;
        case ShaderKind::Pixel:
            return shaderc_fragment_shader;
        default:
            ASSERT(false, "Unrecognized shader kind")
        }
        std::unreachable();
    }

    VkShaderStageFlagBits kindToVkStage(ShaderKind kind)
    {
        switch (kind)
        {
        case ShaderKind::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderKind::Pixel:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        default:
            ASSERT(false, "Unrecognized shader kind")
        }
        std::unreachable();
    }
}

Pipeline Pipeline::Builder::Build()
{
    FinishShaders();
    Pipeline pipeline = Pipeline::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([pipeline](){ Pipeline::Destroy(pipeline); });

    return pipeline;
}

Pipeline::Builder& Pipeline::Builder::SetRenderPass(const RenderPass& renderPass)
{
    Driver::Unpack(renderPass, m_CreateInfo);
    
    return *this;
}

Pipeline::Builder& Pipeline::Builder::AddShader(ShaderKind shaderKind, std::string_view shaderPath)
{
    ASSERT(m_CreateInfo.Device != VK_NULL_HANDLE, "Device is unset")
    ASSERT(std::ranges::find_if(m_ShaderModules,
        [shaderKind](auto& data) { return data.Kind == shaderKind; }) == m_ShaderModules.end(),
        "Shader of that kind is already set")
    
    shaderc_shader_kind shaderCKind = kindToShaderCKind(shaderKind);
    std::vector<u32> shaderSrc = utils::compileShaderToSPIRV(shaderPath, shaderCKind);
    m_ShaderModules.push_back({CreateShader(shaderSrc), shaderKind});    

    return *this;
}

Pipeline::Builder& Pipeline::Builder::FixedFunctionDefaults()
{
    m_CreateInfo.DynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    m_CreateInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_CreateInfo.DynamicStateInfo.dynamicStateCount = (u32)m_CreateInfo.DynamicStates.size();
    m_CreateInfo.DynamicStateInfo.pDynamicStates = m_CreateInfo.DynamicStates.data();

    m_CreateInfo.ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_CreateInfo.ViewportState.viewportCount = 1;
    m_CreateInfo.ViewportState.scissorCount = 1;

    m_CreateInfo.InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_CreateInfo.InputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_CreateInfo.InputAssemblyState.primitiveRestartEnable = VK_FALSE;
    
    m_CreateInfo.VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    m_CreateInfo.RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_CreateInfo.RasterizationState.depthClampEnable = VK_FALSE;
    m_CreateInfo.RasterizationState.depthBiasEnable = VK_FALSE;
    m_CreateInfo.RasterizationState.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
    m_CreateInfo.RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    m_CreateInfo.RasterizationState.lineWidth = 1.0f;
    m_CreateInfo.RasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    m_CreateInfo.RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    m_CreateInfo.MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_CreateInfo.MultisampleState.sampleShadingEnable = VK_FALSE;
    m_CreateInfo.MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    m_CreateInfo.ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                            VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT |
                                                            VK_COLOR_COMPONENT_A_BIT;
    m_CreateInfo.ColorBlendAttachmentState.blendEnable = VK_TRUE;
    m_CreateInfo.ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_CreateInfo.ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_CreateInfo.ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    m_CreateInfo.ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_CreateInfo.ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_CreateInfo.ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    m_CreateInfo.ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_CreateInfo.ColorBlendState.attachmentCount = 1;
    m_CreateInfo.ColorBlendState.pAttachments = &m_CreateInfo.ColorBlendAttachmentState;
    m_CreateInfo.ColorBlendState.logicOpEnable = VK_FALSE;

    m_CreateInfo.PipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    return *this;
}

void Pipeline::Builder::FinishShaders()
{
    ASSERT(!m_ShaderModules.empty(), "No shaders were set")
    for (auto& module : m_ShaderModules)
    {
        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.module = module.Module;
        shaderStageCreateInfo.stage = kindToVkStage(module.Kind);
        shaderStageCreateInfo.pName = "main";

        m_CreateInfo.Shaders.push_back(shaderStageCreateInfo);
        m_CreateInfo.ShaderModules.push_back(module.Module);
    }
}

VkShaderModule Pipeline::Builder::CreateShader(const std::vector<u32>& spirv) const
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = spirv.size() * sizeof(u32);
    shaderModuleCreateInfo.pCode = spirv.data();

    VkShaderModule shaderModule;

    VulkanCheck(vkCreateShaderModule(m_CreateInfo.Device, &shaderModuleCreateInfo, nullptr, &shaderModule),
        "Failed to create shader module");

    return shaderModule;
}

Pipeline Pipeline::Create(const Builder::CreateInfo& createInfo)
{
    Pipeline pipeline = {};
    
    pipeline.m_Device = createInfo.Device;

    VulkanCheck(vkCreatePipelineLayout(createInfo.Device, &createInfo.PipelineLayout, nullptr, &pipeline.m_Layout),
        "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = (u32)createInfo.Shaders.size();
    pipelineCreateInfo.pStages = createInfo.Shaders.data();
    pipelineCreateInfo.pDynamicState = &createInfo.DynamicStateInfo;
    pipelineCreateInfo.pViewportState = &createInfo.ViewportState;
    pipelineCreateInfo.pInputAssemblyState = &createInfo.InputAssemblyState;
    pipelineCreateInfo.pVertexInputState = &createInfo.VertexInputState;
    pipelineCreateInfo.pRasterizationState = &createInfo.RasterizationState;
    pipelineCreateInfo.pMultisampleState = &createInfo.MultisampleState;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &createInfo.ColorBlendState;
    pipelineCreateInfo.layout = pipeline.m_Layout;
    pipelineCreateInfo.renderPass = createInfo.RenderPass;
    pipelineCreateInfo.subpass = 0;

    VulkanCheck(vkCreateGraphicsPipelines(createInfo.Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline.m_Pipeline),
        "Failed to create pipeline");

    for (auto& module : createInfo.ShaderModules)
        vkDestroyShaderModule(createInfo.Device, module, nullptr);

    return pipeline;
}

void Pipeline::Destroy(const Pipeline& pipeline)
{
    vkDestroyPipeline(pipeline.m_Device, pipeline.m_Pipeline, nullptr);
    vkDestroyPipelineLayout(pipeline.m_Device, pipeline.m_Layout, nullptr);
}

void Pipeline::Bind(const CommandBuffer& commandBuffer, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindPipeline(commandBuffer, *this, bindPoint);
}
