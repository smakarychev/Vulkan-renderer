#include "VisibilityPass.h"

#include "Mesh.h"
#include "Renderer.h"
#include "Scene.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    constexpr VkFormat VISIBILITY_BUFFER_FORMAT = VK_FORMAT_R32_UINT;
    constexpr u32 VISIBILITY_BUFFER_CLEAR_VALUE = std::numeric_limits<u32>::max();
}

VisibilityBuffer::VisibilityBuffer(const glm::uvec2& size, const CommandBuffer& cmd)
{
    m_VisibilityImage = Image::Builder()
        .SetExtent({size.x, size.y})
        .SetFormat(VISIBILITY_BUFFER_FORMAT)
        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .BuildManualLifetime();

    ImageSubresource imageSubresource = m_VisibilityImage.CreateSubresource(1, 1);
    
    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = &imageSubresource,
            .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .DestinationStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .DestinationAccess = VK_ACCESS_2_SHADER_READ_BIT,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL})
        .Build();

    Barrier barrier = {};
    barrier.Wait(cmd, layoutTransition);
}

VisibilityBuffer::~VisibilityBuffer()
{
    Image::Destroy(m_VisibilityImage);
}

bool VisibilityPass::Init(const VisibilityPassInitInfo& initInfo)
{
    bool recreated = false;
    if (m_VisibilityBuffer)
    {
        m_VisibilityBuffer.reset();
        ShaderDescriptorSet::Destroy(m_DescriptorSet);
        recreated = true;
    }
    else
    {
        m_Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
            {"../assets/shaders/processed/visibility-buffer/visibility-buffer-vert.shader",
            "../assets/shaders/processed/visibility-buffer/visibility-buffer-frag.shader"},
            "visibility-pipeline",
            *initInfo.DescriptorAllocator, *initInfo.LayoutCache);

        RenderingDetails details = initInfo.RenderingDetails;
        details.ColorFormats = {VISIBILITY_BUFFER_FORMAT};
        
        m_Pipeline = ShaderPipeline::Builder()
            .SetTemplate(m_Template)
            .SetRenderingDetails(details)
            .AlphaBlending(AlphaBlending::None)
            .Build();

        m_SplitBarrierDependency = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .DestinationAccess = VK_ACCESS_2_SHADER_STORAGE_READ_BIT})
            .Build();
        
        for (auto& barrier : m_SplitBarriers)
        {
            barrier = SplitBarrier::Builder().Build();
            barrier.Signal(*initInfo.Cmd, m_SplitBarrierDependency);
        }
    }
    
    m_VisibilityBuffer = std::make_unique<VisibilityBuffer>(initInfo.Size, *initInfo.Cmd);

    m_DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Template)
        .AddBinding("u_camera_buffer", *initInfo.CameraBuffer, sizeof(CameraData), 0)
        .AddBinding("u_position_buffer", initInfo.Scene->GetPositionsBuffer())
        .AddBinding("u_uv_buffer", initInfo.Scene->GetUVsBuffer())
        .AddBinding("u_object_buffer", *initInfo.ObjectsBuffer)
        .AddBinding("u_triangle_buffer", *initInfo.TrianglesBuffer, CullDrawBatch::GetTrianglesSizeBytes(), 0)
        .AddBinding("u_command_buffer", initInfo.Scene->GetMeshletsIndirectBuffer())
        .AddBinding("u_material_buffer", *initInfo.MaterialsBuffer)
        .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
        .BuildManualLifetime();

    initInfo.Scene->ApplyMaterialTextures(m_DescriptorSet);

    return recreated;
}

void VisibilityPass::ShutDown()
{
    if (m_VisibilityBuffer)
    {
        m_VisibilityBuffer.reset();
        ShaderDescriptorSet::Destroy(m_DescriptorSet);
    }
}

void VisibilityPass::RenderVisibility(const VisibilityRenderInfo& renderInfo)
{
    Barrier barrier = {};
    u32 batchCount = 0;
    
    auto preTriangleCull = [&](CommandBuffer& cmd, bool reocclusion, Fence fence)
    {
        ZoneScopedN("Culling");

        CullContext cullContext = {
            .Reocclusion = reocclusion,
            .Cmd = &cmd,
            .FrameNumber = renderInfo.FrameContext->FrameNumber};
    
        renderInfo.SceneCull->CullMeshes(cullContext);
        renderInfo.SceneCull->CullMeshlets(cullContext);

        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_2_HOST_BIT,
                .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .DestinationAccess = VK_ACCESS_2_HOST_READ_BIT})
            .Build();
        barrier.Wait(cmd, dependencyInfo);

        cmd.End();
        cmd.Submit(Driver::GetDevice().GetQueues().Graphics, fence);
    };
    auto preTriangleWaitCPU = [&](CommandBuffer& cmd, Fence fence)
    {
        ZoneScopedN("Fence wait");
        fence.Wait();
        fence.Reset();
        batchCount = renderInfo.SceneCull->ReadBackBatchCount(renderInfo.FrameContext->FrameNumber);
        cmd.Begin();
    };
    auto cull = [&](CommandBuffer& cmd, bool reocclusion)
    {
        ZoneScopedN("Culling");
        CullContext cullContext = {
            .Reocclusion = reocclusion,
            .Cmd = &cmd,
            .FrameNumber = renderInfo.FrameContext->FrameNumber};
    
        renderInfo.SceneCull->CullCompactTrianglesBatch(cullContext);
    };
    auto postCullBatchBarriers = [&](CommandBuffer& cmd)
    {
        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .DestinationAccess = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT})
            .Build();
        barrier.Wait(cmd, dependencyInfo);
    };
    auto render = [&](CommandBuffer& cmd, u32 iteration, bool computeDepthPyramid, bool shouldClear)
    {
        ZoneScopedN("Rendering");

        RenderCommand::SetViewport(cmd, renderInfo.FrameContext->Resolution);
        RenderCommand::SetScissors(cmd, {0, 0}, renderInfo.FrameContext->Resolution);
        RenderCommand::BeginRendering(cmd, iteration == 0 && shouldClear ?
            GetClearRenderingInfo(*renderInfo.DepthBuffer, renderInfo.FrameContext->Resolution) :
            GetLoadRenderingInfo(*renderInfo.DepthBuffer, renderInfo.FrameContext->Resolution));

        RenderCommand::BindIndexBuffer(cmd, renderInfo.SceneCull->GetCullDrawBatch().GetIndicesSingular(), 0);
        RenderScene(cmd, *renderInfo.Scene, *renderInfo.SceneCull, renderInfo.FrameContext->FrameNumber);

        RenderCommand::EndRendering(cmd);

        if (computeDepthPyramid)
            ComputeDepthPyramid(cmd, *renderInfo.DepthPyramid, *renderInfo.DepthBuffer);
    };
    auto triangleCullRenderLoop = [&](CommandBuffer& cmd, bool reocclusion, bool computeDepthPyramid, bool shouldClear)
    {
        renderInfo.SceneCull->ResetSubBatches();
        u32 iterations = std::max(batchCount, 1u);
        for (u32 i = 0; i < iterations; i++)
        {
            m_SplitBarriers[renderInfo.SceneCull->GetBatchCull().GetBatchIndex()].Wait(cmd, m_SplitBarrierDependency);
            m_SplitBarriers[renderInfo.SceneCull->GetBatchCull().GetBatchIndex()].Reset(cmd, m_SplitBarrierDependency);
            cull(cmd, reocclusion);
            postCullBatchBarriers(cmd);
            render(cmd, i, computeDepthPyramid && i == iterations - 1, shouldClear);
            m_SplitBarriers[renderInfo.SceneCull->GetBatchCull().GetBatchIndex()].Signal(cmd, m_SplitBarrierDependency);
            renderInfo.SceneCull->NextSubBatch();
        }
    };
    auto postRenderBarriers = [&](CommandBuffer& cmd)
    {
        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .SourceAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .DestinationAccess = VK_ACCESS_2_SHADER_READ_BIT})
            .Build();
        barrier.Wait(cmd, dependencyInfo);
    };

    Fence fence = Fence::Builder().BuildManualLifetime();
    
    CommandBuffer& cmd = renderInfo.FrameContext->Cmd;
    preTriangleCull(cmd, false, fence);
    preTriangleWaitCPU(cmd, fence);

    renderInfo.SceneCull->BatchIndirectDispatchesBuffersPrepare({
        .Cmd = &cmd,
        .FrameNumber = renderInfo.FrameContext->FrameNumber});
    triangleCullRenderLoop(cmd, false, true, true);

    // triangle-only reocclusion
    triangleCullRenderLoop(cmd, true, true, false);

    // meshlet reocclusion
    preTriangleCull(cmd, true, fence);
    preTriangleWaitCPU(cmd, fence);
    renderInfo.SceneCull->BatchIndirectDispatchesBuffersPrepare({
        .Cmd = &cmd,
        .FrameNumber = renderInfo.FrameContext->FrameNumber});
    triangleCullRenderLoop(cmd, true, false, false);
    postRenderBarriers(cmd);

    Fence::Destroy(fence);
}

RenderingInfo VisibilityPass::GetClearRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const
{
    // todo: fix: direct VKAPI Usage
    VkClearValue colorClear = {};
    colorClear.color.uint32[0] = VISIBILITY_BUFFER_CLEAR_VALUE;
    VkClearValue depthClear = {.depthStencil = {.depth = 0.0f}};
    
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_VisibilityBuffer->GetVisibilityImage().GetImageData(), VK_IMAGE_LAYOUT_GENERAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .ClearValue(colorClear)
        .Build();

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(depthBuffer.GetImageData(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .ClearValue(depthClear)
        .Build();

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(resolution)
        .Build();

    return renderingInfo;
}

RenderingInfo VisibilityPass::GetLoadRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_VisibilityBuffer->GetVisibilityImage().GetImageData(), VK_IMAGE_LAYOUT_GENERAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
        .Build();

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(depthBuffer.GetImageData(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
        .Build();

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(resolution)
        .Build();

    return renderingInfo;
}

void VisibilityPass::ComputeDepthPyramid(const CommandBuffer& cmd, DepthPyramid& depthPyramid, const Image& depthBuffer)
{
    ZoneScopedN("Compute depth pyramid");
    depthPyramid.Compute(depthBuffer, cmd);
}

void VisibilityPass::RenderScene(const CommandBuffer& cmd, const Scene& scene, const SceneCull& sceneCull,
    u32 frameNumber)
{
    ZoneScopedN("Scene render");

    m_Pipeline.BindGraphics(cmd);
    const PipelineLayout& layout = m_Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * frameNumber);
    u32 trianglesOffset = (u32)sceneCull.GetDrawTrianglesOffset(); 
    m_DescriptorSet.BindGraphics(cmd, DescriptorKind::Global, layout, {cameraDataOffset});
    m_DescriptorSet.BindGraphics(cmd, DescriptorKind::Pass, layout, {trianglesOffset});
    m_DescriptorSet.BindGraphics(cmd, DescriptorKind::Material, layout);
    //scene.Bind(cmd);

    RenderCommand::DrawIndexedIndirect(cmd,
        sceneCull.GetDrawCommandsSingular(),
        0, 1, sizeof(IndirectCommand));
    
    //RenderCommand::DrawIndexedIndirectCount(cmd,
    //   sceneCull.GetDrawCommands(), commandsOffset,
    //   sceneCull.GetDrawCount(), 0,
    //   CullDrawBatch::GetCommandCount(), sizeof(IndirectCommand));
}
