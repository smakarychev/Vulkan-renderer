#include "Pipeline.h"

#include "Core/core.h"
#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"
#include "Rendering/Shader.h"

Pipeline Pipeline::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

Pipeline Pipeline::Builder::Build(DeletionQueue& deletionQueue)
{
    PreBuild();
    
    Pipeline pipeline = Pipeline::Create(m_CreateInfo);
    deletionQueue.Enqueue(pipeline);

    return pipeline;
}

Pipeline Pipeline::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return Pipeline::Create(m_CreateInfo);
}

Pipeline::Builder& Pipeline::Builder::SetLayout(PipelineLayout layout)
{
    m_CreateInfo.PipelineLayout = layout;
    
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

Pipeline::Builder& Pipeline::Builder::AddShader(const ShaderModuleSource& shader)
{
    m_CreateInfo.Shaders.push_back(shader);

    return *this;
}

Pipeline::Builder& Pipeline::Builder::SetVertexDescription(const VertexInputDescription& vertexDescription)
{
    m_CreateInfo.VertexDescription = vertexDescription;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::DynamicStates(::DynamicStates states)
{
    m_CreateInfo.DynamicStates = states;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::ClampDepth(bool enable)
{
    m_CreateInfo.ClampDepth = enable;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::DepthMode(::DepthMode depthMode)
{
    m_CreateInfo.DepthMode = depthMode;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::FaceCullMode(::FaceCullMode cullMode)
{
    m_CreateInfo.CullMode = cullMode;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::PrimitiveKind(::PrimitiveKind primitiveKind)
{
    m_CreateInfo.PrimitiveKind = primitiveKind;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::AlphaBlending(::AlphaBlending alphaBlending)
{
    m_CreateInfo.AlphaBlending = alphaBlending;

    return *this;
}

Pipeline::Builder& Pipeline::Builder::UseSpecialization(const PipelineSpecializationInfo& pipelineSpecializationInfo)
{
    m_CreateInfo.ShaderSpecialization = pipelineSpecializationInfo;
        
    return *this;
}

Pipeline::Builder& Pipeline::Builder::UseDescriptorBuffer()
{
    m_CreateInfo.UseDescriptorBuffer = true;

    return *this;
}

void Pipeline::Builder::PreBuild()
{
    if (!m_CreateInfo.IsComputePipeline)
        ASSERT(
            !m_CreateInfo.RenderingDetails.ColorFormats.empty() ||
            m_CreateInfo.RenderingDetails.DepthFormat != Format::Undefined, "No rendering details provided")
}

Pipeline Pipeline::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void Pipeline::Destroy(const Pipeline& pipeline)
{
    Device::Destroy(pipeline.Handle());
}

void Pipeline::BindGraphics(const CommandBuffer& commandBuffer) const
{
    RenderCommand::BindGraphics(commandBuffer, *this);
}

void Pipeline::BindCompute(const CommandBuffer& commandBuffer) const
{
    RenderCommand::BindCompute(commandBuffer, *this);
}
