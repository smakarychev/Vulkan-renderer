#include "VisibilityPass.h"

#include "Renderer.h"
#include "Rendering/RenderingUtils.h"

namespace
{
    constexpr Format VISIBILITY_BUFFER_FORMAT = Format::R32_UINT;
    constexpr u32 VISIBILITY_BUFFER_CLEAR_VALUE = std::numeric_limits<u32>::max();
}

VisibilityBuffer::VisibilityBuffer(const glm::uvec2& size, const CommandBuffer& cmd)
{
    m_VisibilityImage = Image::Builder()
        .SetExtent({size.x, size.y})
        .SetFormat(VISIBILITY_BUFFER_FORMAT)
        .SetUsage(ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::Color)
        .BuildManualLifetime();

    ImageSubresource imageSubresource = m_VisibilityImage.CreateSubresource(1, 1);
    
    DeletionQueue deletionQueue = {};
    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = &imageSubresource,
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General})
        .Build(deletionQueue);

    Barrier barrier = {};
    barrier.Wait(cmd, layoutTransition);
}

VisibilityBuffer::~VisibilityBuffer()
{
    Image::Destroy(m_VisibilityImage);
}

void VisibilityPass::Init(const VisibilityPassInitInfo& initInfo)
{
    if (m_VisibilityBuffer)
    {
        m_VisibilityBuffer.reset();
    }
    else
    {
        m_Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
            {"../assets/shaders/processed/visibility-buffer/visibility-buffer-vert.shader",
            "../assets/shaders/processed/visibility-buffer/visibility-buffer-frag.shader"},
            "visibility-pipeline",
            *initInfo.DescriptorAllocator);

        RenderingDetails details = initInfo.RenderingDetails;
        details.ColorFormats = {VISIBILITY_BUFFER_FORMAT};
        
        m_Pipeline = ShaderPipeline::Builder()
            .SetTemplate(m_Template)
            .SetRenderingDetails(details)
            .AlphaBlending(AlphaBlending::None)
            .Build();

        m_RenderPassGeometry = initInfo.RenderPassGeometry;
        m_RenderPassGeometryCull = initInfo.RenderPassGeometryCull;

        m_DescriptorSet = ShaderDescriptorSet::Builder()
            .SetTemplate(m_Template)
            .AddBinding("u_camera_buffer", *initInfo.CameraBuffer, sizeof(CameraData), 0)
            .AddBinding("u_position_buffer", initInfo.RenderPassGeometry->GetAttributeBuffers().Positions)
            .AddBinding("u_uv_buffer", initInfo.RenderPassGeometry->GetAttributeBuffers().UVs)
            .AddBinding("u_object_buffer", initInfo.RenderPassGeometry->GetRenderObjectsBuffer())
            .AddBinding("u_triangle_buffer", initInfo.RenderPassGeometryCull->GetTriangleBuffer(),
                initInfo.RenderPassGeometryCull->GetTriangleBufferSizeBytes(), 0)
            .AddBinding("u_command_buffer", initInfo.RenderPassGeometry->GetCommandsBuffer())
            .AddBinding("u_material_buffer", initInfo.RenderPassGeometry->GetMaterialsBuffer())
            .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
            .Build();
    }
    
    m_VisibilityBuffer = std::make_unique<VisibilityBuffer>(initInfo.Size, *initInfo.Cmd);

    initInfo.RenderPassGeometry->GetModelCollection().ApplyMaterialTextures(m_DescriptorSet);
}

void VisibilityPass::Shutdown()
{
    if (m_VisibilityBuffer)
        m_VisibilityBuffer.reset();
}

void VisibilityPass::RenderVisibility(const VisibilityRenderInfo& renderInfo)
{
    RenderPassPipelineData renderPassPipelineData{
        .Pipeline = m_Pipeline,
        .Descriptors = m_DescriptorSet};

    u32 cameraDataOffset = u32(renderUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) *
        renderInfo.FrameContext->FrameNumber);
    u32 trianglesOffset = RenderPassGeometryCull::TRIANGLE_OFFSET; 
    DescriptorsOffsets descriptorsOffsets = {};
    descriptorsOffsets[(u32)DescriptorKind::Global] = {cameraDataOffset};
    descriptorsOffsets[(u32)DescriptorKind::Pass] = {trianglesOffset};

    RenderingInfo clearRenderingInfo = GetClearRenderingInfo(*renderInfo.DepthBuffer,
        renderInfo.FrameContext->Resolution, renderInfo.FrameContext);
    RenderingInfo loadRenderingInfo = GetLoadRenderingInfo(*renderInfo.DepthBuffer,
        renderInfo.FrameContext->Resolution, renderInfo.FrameContext);
    
    m_RenderPassGeometryCull->CullRender({
        .Cmd = &renderInfo.FrameContext->Cmd,
        .DeletionQueue = &renderInfo.FrameContext->DeletionQueue,
        .FrameNumber = renderInfo.FrameContext->FrameNumber,
        .Resolution = renderInfo.FrameContext->Resolution,
        .RenderingPipeline = &renderPassPipelineData,
        .DescriptorsOffsets = descriptorsOffsets,
        .ClearRenderingInfo = &clearRenderingInfo,
        .CopyRenderingInfo = &loadRenderingInfo,
        .DepthBuffer = renderInfo.DepthBuffer});
}

RenderingInfo VisibilityPass::GetClearRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution,
    FrameContext* frameContext) const
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_VisibilityBuffer->GetVisibilityImage(), ImageLayout::General)
        .LoadStoreOperations(AttachmentLoad::Clear, AttachmentStore::Store)
        .ClearValue(glm::uvec4(VISIBILITY_BUFFER_CLEAR_VALUE, 0u, 0u, 0u))
        .Build(frameContext->DeletionQueue);

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(depthBuffer, ImageLayout::DepthAttachment)
        .LoadStoreOperations(AttachmentLoad::Clear, AttachmentStore::Store)
        .ClearValue(0.0f)
        .Build(frameContext->DeletionQueue);

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(resolution)
        .Build(frameContext->DeletionQueue);

    return renderingInfo;
}

RenderingInfo VisibilityPass::GetLoadRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution,
    FrameContext* frameContext) const
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_VisibilityBuffer->GetVisibilityImage(), ImageLayout::General)
        .LoadStoreOperations(AttachmentLoad::Load, AttachmentStore::Store)
        .Build(frameContext->DeletionQueue);

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(depthBuffer, ImageLayout::DepthAttachment)
        .LoadStoreOperations(AttachmentLoad::Load, AttachmentStore::Store)
        .Build(frameContext->DeletionQueue);

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(resolution)
        .Build(frameContext->DeletionQueue);

    return renderingInfo;
}
