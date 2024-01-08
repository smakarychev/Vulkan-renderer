#include "Pipeline.h"

#include <shaderc/shaderc.h>

#include "Core/core.h"
#include "Driver.h"
#include "RenderCommand.h"
#include "utils.h"
#include "VulkanUtils.h"

namespace
{
    VkShaderStageFlagBits kindToVkStage(ShaderKind kind)
    {
        switch (kind)
        {
        case ShaderKind::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderKind::Pixel:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderKind::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            ASSERT(false, "Unrecognized shader kind")
        }
        std::unreachable();
    }
}

PipelineLayout PipelineLayout::Builder::Build()
{
    PipelineLayout layout = PipelineLayout::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([layout]() { PipelineLayout::Destroy(layout); });

    return layout;
}

PipelineLayout::Builder& PipelineLayout::Builder::SetPushConstants(const std::vector<PushConstantDescription>& pushConstants)
{
    for (auto& pushConstant : pushConstants)
        Driver::Unpack(pushConstant, m_CreateInfo);

    return *this;
}

PipelineLayout::Builder& PipelineLayout::Builder::SetDescriptorLayouts(const std::vector<DescriptorSetLayout*>& layouts)
{
    for (auto& layout : layouts)
        Driver::Unpack(*layout, m_CreateInfo);

    return *this;
}

PipelineLayout PipelineLayout::Create(const Builder::CreateInfo& createInfo)
{
    PipelineLayout layout = {};

    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pushConstantRangeCount = (u32)createInfo.PushConstantRanges.size();
    layoutCreateInfo.pPushConstantRanges = createInfo.PushConstantRanges.data();
    layoutCreateInfo.setLayoutCount = (u32)createInfo.DescriptorSetLayouts.size();
    layoutCreateInfo.pSetLayouts = createInfo.DescriptorSetLayouts.data();

    VulkanCheck(vkCreatePipelineLayout(Driver::DeviceHandle(), &layoutCreateInfo, nullptr, &layout.m_Layout),
        "Failed to create pipeline layout");

    return layout;
}

void PipelineLayout::Destroy(const PipelineLayout& pipelineLayout)
{
    vkDestroyPipelineLayout(Driver::DeviceHandle(), pipelineLayout.m_Layout, nullptr);
}

Pipeline Pipeline::Builder::Build()
{
    PreBuild();
    
    Pipeline pipeline = Pipeline::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([pipeline](){ Pipeline::Destroy(pipeline); });

    return pipeline;
}

Pipeline Pipeline::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return Pipeline::Create(m_CreateInfo);
}

Pipeline::Builder& Pipeline::Builder::SetLayout(const PipelineLayout& layout)
{
    Driver::Unpack(layout, m_CreateInfo);
    
    return *this;
}

Pipeline::Builder& Pipeline::Builder::SetRenderingDetails(const RenderingDetails& renderingDetails)
{
    ASSERT(!m_CreateInfo.IsComputePipeline, "Compute pipeline does not need rendering details")
    m_CreateInfo.RenderingDetails = renderingDetails;
    
    return *this;
}

Pipeline::Builder& Pipeline::Builder::IsComputePipeline(bool isCompute)
{
    m_CreateInfo.IsComputePipeline = isCompute;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::AddShader(const ShaderModuleData& shaderModuleData)
{
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.module = shaderModuleData.Module;
    shaderStageCreateInfo.stage = kindToVkStage(shaderModuleData.Kind);
    shaderStageCreateInfo.pName = "main";

    m_CreateInfo.Shaders.push_back(shaderStageCreateInfo);

    return *this;
}

Pipeline::Builder& Pipeline::Builder::FixedFunctionDefaults()
{
    m_CreateInfo.DynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    m_CreateInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    m_CreateInfo.ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_CreateInfo.ViewportState.viewportCount = 1;
    m_CreateInfo.ViewportState.scissorCount = 1;

    // topology is set later
    m_CreateInfo.InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_CreateInfo.InputAssemblyState.primitiveRestartEnable = VK_FALSE;
    
    m_CreateInfo.VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    m_CreateInfo.RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_CreateInfo.RasterizationState.depthClampEnable = VK_FALSE;
    m_CreateInfo.RasterizationState.depthBiasEnable = VK_FALSE;
    m_CreateInfo.RasterizationState.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
    m_CreateInfo.RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    m_CreateInfo.RasterizationState.lineWidth = 1.0f;
    m_CreateInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
    m_CreateInfo.RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    m_CreateInfo.MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_CreateInfo.MultisampleState.sampleShadingEnable = VK_FALSE;
    m_CreateInfo.MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    m_CreateInfo.DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_CreateInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    m_CreateInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    m_CreateInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    m_CreateInfo.DepthStencilState.depthBoundsTestEnable = VK_FALSE;
    m_CreateInfo.DepthStencilState.stencilTestEnable = VK_FALSE;

    m_CreateInfo.ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                            VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT |
                                                            VK_COLOR_COMPONENT_A_BIT;

    m_CreateInfo.ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_CreateInfo.ColorBlendState.logicOpEnable = VK_FALSE;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::SetVertexDescription(const VertexInputDescription& vertexDescription)
{
    m_VertexInputDescription = vertexDescription;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::PrimitiveKind(::PrimitiveKind primitiveKind)
{
    m_PrimitiveKind = primitiveKind;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::AlphaBlending(::AlphaBlending alphaBlending)
{
    m_AlphaBlending = alphaBlending;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::UseSpecialization(const PipelineSpecializationInfo& pipelineSpecializationInfo)
{
    m_PipelineSpecializationInfo = pipelineSpecializationInfo;
        
    return *this;
}

void Pipeline::Builder::PreBuild()
{
    m_CreateInfo.DynamicStateInfo.dynamicStateCount = (u32)m_CreateInfo.DynamicStates.size();
    m_CreateInfo.DynamicStateInfo.pDynamicStates = m_CreateInfo.DynamicStates.data();

    m_CreateInfo.InputAssemblyState.topology = vkUtils::vkTopologyByPrimitiveKind(m_PrimitiveKind);
    
    m_CreateInfo.VertexInputState.vertexBindingDescriptionCount = (u32)m_VertexInputDescription.Bindings.size();
    m_CreateInfo.VertexInputState.pVertexBindingDescriptions = m_VertexInputDescription.Bindings.data();
    m_CreateInfo.VertexInputState.vertexAttributeDescriptionCount = (u32)m_VertexInputDescription.Attributes.size();
    m_CreateInfo.VertexInputState.pVertexAttributeDescriptions = m_VertexInputDescription.Attributes.data();

    m_CreateInfo.ColorBlendState.attachmentCount = 1;
    m_CreateInfo.ColorBlendState.pAttachments = &m_CreateInfo.ColorBlendAttachmentState;

    ChooseBlendingMode();

    m_CreateInfo.ShaderSpecializationInfos.reserve(m_CreateInfo.Shaders.size());

    u32 entriesOffset = 0;
    for (auto& shader : m_CreateInfo.Shaders)
    {
        VkSpecializationInfo shaderSpecializationInfo = {};
        for (const auto& specialization : m_PipelineSpecializationInfo.ShaderSpecializations)
            if ((shader.stage & specialization.ShaderStages) != 0)
                m_CreateInfo.ShaderSpecializationEntries.push_back(specialization.SpecializationEntry);

        shaderSpecializationInfo.dataSize = m_PipelineSpecializationInfo.Buffer.size();
        shaderSpecializationInfo.pData = m_PipelineSpecializationInfo.Buffer.data();
        shaderSpecializationInfo.mapEntryCount = (u32)m_CreateInfo.ShaderSpecializationEntries.size() - entriesOffset;
        shaderSpecializationInfo.pMapEntries = m_CreateInfo.ShaderSpecializationEntries.data() + entriesOffset;

        m_CreateInfo.ShaderSpecializationInfos.push_back(shaderSpecializationInfo);
        shader.pSpecializationInfo = &m_CreateInfo.ShaderSpecializationInfos.back();

        entriesOffset = (u32)m_CreateInfo.ShaderSpecializationEntries.size();
    }

    if (!m_CreateInfo.IsComputePipeline)
        ASSERT(!m_CreateInfo.RenderingDetails.ColorFormats.empty(), "No rendering details provided")
}

void Pipeline::Builder::ChooseBlendingMode()
{
    switch (m_AlphaBlending)
    {
    case AlphaBlending::None:
        m_CreateInfo.ColorBlendAttachmentState.blendEnable = VK_FALSE;
        break;
    case AlphaBlending::Over:
        m_CreateInfo.ColorBlendAttachmentState.blendEnable = VK_TRUE;
        m_CreateInfo.ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        m_CreateInfo.ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        m_CreateInfo.ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        m_CreateInfo.ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        m_CreateInfo.ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        m_CreateInfo.ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    default:
        ASSERT(false, "Unsupported blending mode")
    }
}

Pipeline Pipeline::Create(const Builder::CreateInfo& createInfo)
{
    if (createInfo.IsComputePipeline)
        return CreateComputePipeline(createInfo);
    return CreateGraphicsPipeline(createInfo);
}

void Pipeline::Destroy(const Pipeline& pipeline)
{
    vkDestroyPipeline(Driver::DeviceHandle(), pipeline.m_Pipeline, nullptr);
}

void Pipeline::BindGraphics(const CommandBuffer& commandBuffer)
{
    RenderCommand::BindPipeline(commandBuffer, *this, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void Pipeline::BindCompute(const CommandBuffer& commandBuffer)
{
    RenderCommand::BindPipeline(commandBuffer, *this, VK_PIPELINE_BIND_POINT_COMPUTE);
}

Pipeline Pipeline::CreateGraphicsPipeline(const Builder::CreateInfo& createInfo)
{
    Pipeline pipeline = {};

    VkPipelineRenderingCreateInfo renderingCreateInfo = {};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = (u32)createInfo.RenderingDetails.ColorFormats.size();
    renderingCreateInfo.pColorAttachmentFormats = createInfo.RenderingDetails.ColorFormats.data();
    renderingCreateInfo.depthAttachmentFormat = createInfo.RenderingDetails.DepthFormat;
    
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
    pipelineCreateInfo.pDepthStencilState = &createInfo.DepthStencilState;
    pipelineCreateInfo.pColorBlendState = &createInfo.ColorBlendState;
    pipelineCreateInfo.layout = createInfo.Layout;
    pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
    pipelineCreateInfo.subpass = 0;

    pipelineCreateInfo.pNext = &renderingCreateInfo;

    VulkanCheck(vkCreateGraphicsPipelines(Driver::DeviceHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline.m_Pipeline),
        "Failed to create graphics pipeline");

    return pipeline;
}

Pipeline Pipeline::CreateComputePipeline(const Builder::CreateInfo& createInfo)
{
    Pipeline pipeline = {};

    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage = createInfo.Shaders.front();
    pipelineCreateInfo.layout = createInfo.Layout;

    VulkanCheck(vkCreateComputePipelines(Driver::DeviceHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline.m_Pipeline),
        "Failed to create compute pipeline");

    return pipeline;
}
