#include "VisibilityPass.h"

#include "Mesh.h"
#include "Renderer.h"
#include "Scene.h"
#include "Core/ProfilerContext.h"
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

    PipelineImageBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .DependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .Image = &m_VisibilityImage,
        .ImageSourceMask = VK_ACCESS_SHADER_WRITE_BIT,
        .ImageDestinationMask = VK_ACCESS_SHADER_READ_BIT,
        .ImageSourceLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .ImageDestinationLayout = VK_IMAGE_LAYOUT_GENERAL,
        .ImageAspect = VK_IMAGE_ASPECT_COLOR_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);    
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
        ShaderDescriptorSet::Destroy(m_DescriptorSet);
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
            .CompatibleWithVertex(VertexP3N3UV2::GetInputDescriptionDI())
            .Build();

        m_CullSemaphore = TimelineSemaphore::Builder().Build();
        m_RenderSemaphore = TimelineSemaphore::Builder().Build();
    }
    
    m_VisibilityBuffer = std::make_unique<VisibilityBuffer>(initInfo.Size, *initInfo.Cmd);

    m_DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Template)
        .AddBinding("u_camera_buffer", *initInfo.CameraBuffer, sizeof(CameraData), 0)
        .AddBinding("u_object_buffer", *initInfo.ObjectsBuffer)
        .AddBinding("u_triangle_buffer", *initInfo.TrianglesBuffer, CullDrawBatch::GetTrianglesSizeBytes(), 0)
        .AddBinding("u_command_buffer", *initInfo.CommandsBuffer, CullDrawBatch::GetCommandsSizeBytes(), 0)
        .BuildManualLifetime();
}

void VisibilityPass::ShutDown()
{
    if (m_VisibilityBuffer)
        m_VisibilityBuffer.reset();
}

void VisibilityPass::RenderVisibility(const VisibilityRenderInfo& renderInfo)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Visiblity pass")
    ZoneScopedN("Visiblity pass");

    u64 renderTimeline = m_RenderSemaphore.GetTimeline();
    u64 cullTimeline = m_CullSemaphore.GetTimeline();

    u32 batchCount = 0;

    Fence preTriangleFence = Fence::Builder().BuildManualLifetime();
    auto preTriangleCull = [&](u64 waitValue, bool reocclusion, Fence fence)
    {
        ZoneScopedN("Culling");
        const CommandBuffer& cmd = renderInfo.FrameContext->ComputeCommandBuffers.GetBuffer();

        renderInfo.SceneCull->CullMeshes(*renderInfo.FrameContext, reocclusion);
        renderInfo.SceneCull->CullMeshlets(*renderInfo.FrameContext, reocclusion);
        
        PipelineBarrierInfo barrierInfo = {
            .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .PipelineDestinationMask = VK_PIPELINE_STAGE_HOST_BIT,
            .AccessSourceMask = VK_ACCESS_SHADER_WRITE_BIT,
            .AccessDestinationMask = VK_ACCESS_HOST_READ_BIT
        };
        RenderCommand::CreateBarrier(cmd, barrierInfo);
        
        cmd.End();
        cmd.Submit(Driver::GetDevice().GetQueues().Compute,
            BufferSubmitTimelineSyncInfo{
                .WaitStages = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}, 
                .WaitSemaphores = {&m_RenderSemaphore},
                .WaitValues = {waitValue},
                .Fence = &fence});
    };
    auto preTriangleWaitCPU = [&](Fence fence)
    {
        ZoneScopedN("Fence wait");
        fence.Wait();
        fence.Reset();
        batchCount = renderInfo.SceneCull->ReadBackBatchCount(*renderInfo.FrameContext);
        
        renderInfo.FrameContext->ComputeCommandBuffers.NextIndex();
        renderInfo.FrameContext->ComputeCommandBuffers.GetBuffer().Begin();
    };
    
    auto cull = [&](u64 waitValue, bool reocclusion)
    {
        ZoneScopedN("Culling");
        const CommandBuffer& cmd = renderInfo.FrameContext->ComputeCommandBuffers.GetBuffer();
        
        renderInfo.SceneCull->CullCompactTrianglesBatch(*renderInfo.FrameContext, {reocclusion});
        
        cullTimeline++;
        cmd.End();
        cmd.Submit(Driver::GetDevice().GetQueues().Compute,
            BufferSubmitTimelineSyncInfo{
                .WaitStages = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}, 
                .WaitSemaphores = {&m_RenderSemaphore},
                .WaitValues = {waitValue},
                .SignalSemaphores = {&m_CullSemaphore},
                .SignalValues = {cullTimeline}});

        renderInfo.FrameContext->ComputeCommandBuffers.NextIndex();
        renderInfo.FrameContext->ComputeCommandBuffers.GetBuffer().Begin();
    };
    auto render = [&](u64 waitValue, u32 iteration, bool computeDepthPyramid, bool shouldClear)
    {
        ZoneScopedN("Rendering");
        const CommandBuffer& cmd = renderInfo.FrameContext->GraphicsCommandBuffers.GetBuffer();

        RenderCommand::SetViewport(cmd, renderInfo.FrameContext->Resolution);
        RenderCommand::SetScissors(cmd, {0, 0}, renderInfo.FrameContext->Resolution);
        RenderCommand::BeginRendering(cmd, iteration == 0 && shouldClear ?
            GetClearRenderingInfo(*renderInfo.DepthBuffer, renderInfo.FrameContext->Resolution) :
            GetLoadRenderingInfo(*renderInfo.DepthBuffer, renderInfo.FrameContext->Resolution));

        RenderCommand::BindIndexBuffer(cmd, renderInfo.SceneCull->GetCullDrawBatch().GetIndices(), 0);
        RenderScene(cmd, *renderInfo.Scene, *renderInfo.SceneCull, renderInfo.FrameContext->FrameNumber);

        RenderCommand::EndRendering(cmd);

        if (computeDepthPyramid)
            ComputeDepthPyramid(cmd, *renderInfo.DepthPyramid, *renderInfo.DepthBuffer);
        
        renderTimeline++;
        cmd.End();
        cmd.Submit(Driver::GetDevice().GetQueues().Graphics,
            BufferSubmitTimelineSyncInfo{
                .WaitStages = {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT}, 
                .WaitSemaphores = {&m_CullSemaphore},
                .WaitValues = {waitValue},
                .SignalSemaphores = {&m_RenderSemaphore},
                .SignalValues = {renderTimeline}});

        renderInfo.FrameContext->GraphicsCommandBuffers.NextIndex();
        renderInfo.FrameContext->GraphicsCommandBuffers.GetBuffer().Begin();
    };
    auto triangleCullRenderLoop = [&](bool reocclusion, bool computeDepthPyramid, bool shouldClear)
    {
        renderInfo.SceneCull->ResetSubBatches();
        cull(renderTimeline, reocclusion);
        u32 iterations = std::max(1u, batchCount);
        for (u32 i = 0; i < iterations; i++)
        {
            render(cullTimeline, i, computeDepthPyramid && i == iterations - 1, shouldClear);
            renderInfo.SceneCull->NextSubBatch();
            if (i != iterations - 1)
                cull(renderTimeline - 1, reocclusion);
        }
    };
    auto postRenderBarriers = [&]()
    {
        PipelineBarrierInfo barrierInfo = {
            .PipelineSourceMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .PipelineDestinationMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .AccessSourceMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .AccessDestinationMask = VK_ACCESS_SHADER_READ_BIT};
        RenderCommand::CreateBarrier(renderInfo.FrameContext->GraphicsCommandBuffers.GetBuffer(), barrierInfo);
    };

    preTriangleCull(renderTimeline, false, preTriangleFence);
    preTriangleWaitCPU(preTriangleFence);
    renderInfo.SceneCull->BatchIndirectDispatchesBuffersPrepare(*renderInfo.FrameContext);
    triangleCullRenderLoop(false, true, true);

    // triangle-only reocclusion
    triangleCullRenderLoop(true, true, false);

    // meshlet reocclusion
    preTriangleCull(renderTimeline, true, preTriangleFence);
    preTriangleWaitCPU(preTriangleFence);
    renderInfo.SceneCull->BatchIndirectDispatchesBuffersPrepare(*renderInfo.FrameContext);
    triangleCullRenderLoop(true, false, false);
    postRenderBarriers();
   
    Fence::Destroy(preTriangleFence);
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

    m_Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * frameNumber);
    u32 trianglesOffset = (u32)sceneCull.GetDrawTrianglesOffset(); 
    u32 commandsOffset = (u32)sceneCull.GetDrawCommandsOffset();  
    m_DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset});
    m_DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_DescriptorSet.Bind(cmd, DescriptorKind::Material, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {commandsOffset, trianglesOffset});
    scene.Bind(cmd);
    
    RenderCommand::DrawIndexedIndirectCount(cmd,
       sceneCull.GetDrawCommands(), commandsOffset,
       sceneCull.GetDrawCount(), 0,
       CullDrawBatch::GetCommandCount(), sizeof(IndirectCommand));
}
