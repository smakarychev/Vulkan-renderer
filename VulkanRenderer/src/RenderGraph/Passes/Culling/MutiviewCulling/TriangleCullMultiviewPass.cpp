#include "TriangleCullMultiviewPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

TriangleCullPrepareMultiviewPass::TriangleCullPrepareMultiviewPass(RG::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/prepare-dispatches-comp.shader"},
        "Pass.TriangleCull.Multiview.PrepareDispatch", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(prepareDispatchTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void TriangleCullPrepareMultiviewPass::AddToGraph(RG::Graph& renderGraph,
    const TriangleCullPrepareMultiviewPassExecutionInfo& info)
{
    using namespace RG;

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            RgUtils::readWriteCullTrianglePrepareMultiview(*info.MultiviewResource, renderGraph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            passData.PipelineData = &m_PipelineData;
            
            graph.GetBlackboard().Register(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            GPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            
            auto* multiview = passData.MultiviewResource;

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            RgUtils::updateCullTrianglePrepareMultiviewBindings(resourceDescriptors, resources, *multiview);
            struct PushConstants
            {
                u32 CommandsPerBatchCount;
                u32 CommandsMultiplier;
                u32 LocalGroupX;
                u32 GeometryCount;
                u32 MeshletViewCount;
                u32 MaxDispatches;
            };

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            PushConstants pushConstants = {
                .CommandsPerBatchCount = TriangleCullMultiviewTraits::CommandCount(),
                .CommandsMultiplier = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .LocalGroupX = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .GeometryCount = multiview->MeshletCull->GeometryCount,
                .MeshletViewCount = multiview->MeshletCull->ViewCount,
                .MaxDispatches = multiview->MaxDispatches};
            
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            
            RenderCommand::Dispatch(cmd,
                {multiview->MaxDispatches, 1, 1},
                {64, 1, 1});

            RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                .MemoryDependency({
                    .SourceStage = PipelineStage::ComputeShader,
                    .DestinationStage = PipelineStage::Host,
                    .SourceAccess = PipelineAccess::WriteShader,
                    .DestinationAccess = PipelineAccess::ReadHost})
                .Build(frameContext.DeletionQueue));

            multiview->MeshletCull->Multiview->UpdateBatchIterationCount();
        });
}

TriangleCullMultiviewPass::TriangleCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const TriangleCullMultiviewPassInitInfo& info)
        : m_Name(name), m_Stage(info.Stage)
{
    ShaderPipelineTemplate* triangleCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
       "../assets/shaders/processed/render-graph/culling/multiview/triangle-cull-comp.shader"},
       "Pass.TriangleCull.Multiview", renderGraph.GetArenaAllocators());

    ShaderPipelineTemplate* trianglePrepareDrawsTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/prepare-draws-comp.shader"},
        "Pass.TriangleCull.Multiview.PrepareDraw", renderGraph.GetArenaAllocators());

    /* init culling data */
    for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
    {
        m_CullPipelines[i].Pipeline = ShaderPipeline::Builder()
            .SetTemplate(triangleCullTemplate)
            .AddSpecialization("REOCCLUSION", info.Stage == CullStage::Reocclusion)
            .AddSpecialization("SINGLE_PASS", info.Stage == CullStage::Single)
            .UseDescriptorBuffer()
            .Build();

        m_CullPipelines[i].SamplerDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(triangleCullTemplate, DescriptorAllocatorKind::Samplers)
            .ExtractSet(0)
            .Build();

        m_CullPipelines[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(triangleCullTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();

        m_PreparePipelines[i].Pipeline = ShaderPipeline::Builder()
            .SetTemplate(trianglePrepareDrawsTemplate)
            .UseDescriptorBuffer()
            .Build();

        m_PreparePipelines[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(trianglePrepareDrawsTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();
    }

    /* init draw data */
    for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
    {
        m_DrawPipelines[i].Pipelines.reserve(info.MultiviewData->Views().size());
        m_DrawPipelines[i].ImmutableSamplerDescriptors.reserve(info.MultiviewData->Views().size());
        m_DrawPipelines[i].ResourceDescriptors.reserve(info.MultiviewData->Views().size());
        m_DrawPipelines[i].MaterialDescriptors.reserve(info.MultiviewData->Views().size());
        for (auto& view : info.MultiviewData->Views())
        {
            if (!view.Static.CullTriangles)
                continue;
            
            ASSERT(!view.Static.MaterialDescriptors.has_value() ||
                enumHasAll(view.Static.DrawFeatures,
                    RG::DrawFeatures::Materials |
                    RG::DrawFeatures::Textures),    
                "If 'MaterialDescriptors' are provided, the 'DrawFeatures' must include 'Materials' and 'Textures'")

            m_DrawPipelines[i].Pipelines.push_back(*view.Static.DrawTrianglesPipeline);
            
            ShaderDescriptors immutableSamplers = {};
            if (view.Static.MaterialDescriptors.has_value())
                immutableSamplers = ShaderDescriptors::Builder()
                    .SetTemplate(view.Static.DrawTrianglesPipeline->GetTemplate(), DescriptorAllocatorKind::Samplers)
                    .ExtractSet(0)
                    .Build();
            
            ShaderDescriptors resourceDescriptors = ShaderDescriptors::Builder()
                .SetTemplate(m_DrawPipelines[i].Pipelines.back().GetTemplate(), DescriptorAllocatorKind::Resources)
                .ExtractSet(1)
                .Build(); 

            m_DrawPipelines[i].ResourceDescriptors.push_back(resourceDescriptors);
            if (view.Static.MaterialDescriptors.has_value())
            {
                m_DrawPipelines[i].MaterialDescriptors.push_back(**view.Static.MaterialDescriptors);
                m_DrawPipelines[i].ImmutableSamplerDescriptors.push_back(immutableSamplers);
            }
        }
    }

    /* init synchronization */
    for (auto& splitBarrier : m_SplitBarriers)
        splitBarrier = SplitBarrier::Builder().Build();
    m_SplitBarrierDependency = DependencyInfo::Builder()
        .MemoryDependency({
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::PixelShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadStorage})
        .Build();
}

void TriangleCullMultiviewPass::AddToGraph(RG::Graph& renderGraph, const TriangleCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassDataPrivate>(m_Name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            if (m_Stage != CullStage::Cull)
            {
                for (u32 i = 0; i < info.MultiviewResource->TriangleViewCount; i++)
                {
                    u32 meshletIndex = info.MultiviewResource->MeshletViewIndices[i];
                    info.MultiviewResource->MeshletCull->HiZs[meshletIndex] =
                        info.MultiviewResource->MeshletCull->Multiview->Views()[meshletIndex]
                            .Static.HiZContext->GetHiZResource();
                }
            }

            RgUtils::readWriteCullTriangleMultiview(*info.MultiviewResource, graph);
            RgUtils::readWriteCullTrianglePrepareMultiview(*info.MultiviewResource, graph);

            passData.MultiviewResource = info.MultiviewResource;
            // todo: currently it is done because I change onLoad to load at some point, and
            // because the execution is deferred, at the moment of execution onLoad equals to load regardless
            // of its original value
            passData.TriangleDrawInfos.reserve(info.MultiviewResource->TriangleViewCount);
            for (auto& v : info.MultiviewResource->Multiview->TriangleViews())
                passData.TriangleDrawInfos.push_back(v.Dynamic.DrawInfo);
            passData.PreparePipelines = &m_PreparePipelines;
            passData.CullPipelines = &m_CullPipelines;
            passData.DrawPipelines = &m_DrawPipelines;
            passData.SplitBarriers = &m_SplitBarriers;
            passData.SplitBarrierDependency = &m_SplitBarrierDependency;

            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources.reserve(info.MultiviewResource->TriangleViewCount);
            for (u32 i = 0; i < info.MultiviewResource->TriangleViewCount; i++)
                passDataPublic.DrawAttachmentResources.push_back(info.MultiviewResource->AttachmentResources[i]);
            graph.GetBlackboard().Update(m_Name.Hash(), passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Triangle Cull Draw Multiview")
            GPU_PROFILE_FRAME("Triangle Cull Draw Multiview")
            return;

            using enum DrawFeatures;

            auto* multiview = passData.MultiviewResource;
            auto* multiviewData = multiview->MeshletCull->Multiview;

            auto createRenderingInfo = [&](bool canClear, u32 viewIndex)
            {
                auto& view = multiviewData->TriangleViews()[viewIndex];
                auto&& [staticV, dynamicV] = view;
                
                RenderingInfo::Builder renderingInfoBuilder = RenderingInfo::Builder()
                    .SetResolution(dynamicV.Resolution);
                auto& colors = passData.TriangleDrawInfos[viewIndex].Attachments.Colors;
                auto& depth = passData.TriangleDrawInfos[viewIndex].Attachments.Depth;
                for (u32 attachmentIndex = 0; attachmentIndex < colors.size(); attachmentIndex++)
                {
                    auto description = colors[attachmentIndex].Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    const Texture& colorTexture = resources.GetTexture(colors[attachmentIndex].Resource);
                    renderingInfoBuilder.AddAttachment(RenderingAttachment::Builder(description)
                        .FromImage(colorTexture, ImageLayout::Attachment)
                        .Build(frameContext.DeletionQueue));
                }
                if (depth.has_value())
                {
                    auto description = depth->Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    const Texture& depthTexture = resources.GetTexture(depth->Resource);
                    renderingInfoBuilder.AddAttachment(RenderingAttachment::Builder(description)
                        .FromImage(depthTexture, ImageLayout::DepthAttachment)
                        .Build(frameContext.DeletionQueue));
                }

                RenderingInfo renderingInfo = renderingInfoBuilder.Build(frameContext.DeletionQueue);

                return renderingInfo;
            };
            auto waitOnBarrier = [&](u32 batchIndex, u32 index)
            {
                if (index < TriangleCullMultiviewTraits::MAX_BATCHES)
                    return;

                passData.SplitBarriers->at(batchIndex).Wait(frameContext.Cmd, *passData.SplitBarrierDependency);
                passData.SplitBarriers->at(batchIndex).Reset(frameContext.Cmd, *passData.SplitBarrierDependency);
            };
            auto signalBarrier = [&](u32 batchIndex)
            {
                passData.SplitBarriers->at(batchIndex).Signal(frameContext.Cmd, *passData.SplitBarrierDependency);
            };

            resources.GetBuffer(multiview->ViewSpans, multiviewData->TriangleViewSpans().data(),
                multiviewData->TriangleViewSpans().size() * sizeof(CullMultiviewData::ViewSpan), 0,
                *frameContext.ResourceUploader);
            std::vector<CullViewDataGPU> views = multiviewData->CreateMultiviewGPUTriangles();
                resources.GetBuffer(multiview->Views, views.data(), views.size() * sizeof(CullViewDataGPU), 0,
                    *frameContext.ResourceUploader);
            for (u32 i = 0; i < multiview->TriangleViewCount; i++)
            {
                auto& view = multiviewData->TriangleViews()[i];
                CameraGPU cameraGPU = CameraGPU::FromCamera(*view.Dynamic.Camera, view.Dynamic.Resolution);
                resources.GetBuffer(multiview->Cameras[i], cameraGPU, *frameContext.ResourceUploader);
            }

            /* update all bindings */
            for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
            {
                auto& cullSamplerDescriptors = passData.CullPipelines->at(i).SamplerDescriptors;
                Sampler hizSampler = multiview->MeshletCull->HiZSampler;
                cullSamplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                    multiview->MeshletCull->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

                std::vector<ShaderDescriptors> drawDescriptors;
                RgUtils::updateCullTriangleMultiviewBindings(
                    passData.CullPipelines->at(i).ResourceDescriptors,
                    passData.PreparePipelines->at(i).ResourceDescriptors,
                    passData.DrawPipelines->at(i).ResourceDescriptors,
                    resources, *multiview, i);
            }

            /* if there are no batches, we clear the render target (if needed) */
            if (multiviewData->GetBatchIterationCount() == 0)
            {
                auto& cmd = frameContext.Cmd;

                for (u32 i = 0; i < multiview->TriangleViewCount; i++)
                {
                    u32 meshletIndex = multiview->MeshletViewIndices[i];
                    auto& view = multiviewData->Views()[meshletIndex];

                    RenderCommand::BeginRendering(cmd, createRenderingInfo(true, i));
                    RenderCommand::SetViewport(cmd, view.Dynamic.Resolution);
                    RenderCommand::SetScissors(cmd, {0, 0}, view.Dynamic.Resolution);
                    RenderCommand::EndRendering(cmd);
                }

                return;
            }

            /* main cull-draw loop */
            for (u32 geometryIndex = 0; geometryIndex < multiviewData->Geometries().size(); geometryIndex++)
            {
                for (u32 batchIteration = 0; batchIteration < multiviewData->GetBatchIterationCount(); batchIteration++)
                {
                    u32 batchIndex = batchIteration % TriangleCullMultiviewTraits::MAX_BATCHES;

                    waitOnBarrier(batchIndex, batchIteration);

                    /* cull */
                    {
                        auto& pipeline = passData.CullPipelines->at(batchIndex).Pipeline;
                        auto& samplerDescriptors = passData.CullPipelines->at(batchIndex).SamplerDescriptors;
                        auto& resourceDescriptors = passData.CullPipelines->at(batchIndex).ResourceDescriptors;

                        struct PushConstants
                        {
                            u32 CommandOffset;
                            u32 MaxCommandIndex;
                            u32 GeometryIndex;
                            u32 ViewCount;
                        };
                        PushConstants pushConstants = {
                            .CommandOffset = batchIteration * TriangleCullMultiviewTraits::CommandCount(),
                            .MaxCommandIndex = TriangleCullMultiviewTraits::CommandCount(),
                            .GeometryIndex = geometryIndex,
                            .ViewCount = multiview->MeshletCull->ViewCount};
                        auto& cmd = frameContext.Cmd;
                        pipeline.BindCompute(cmd);
                        RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
                        samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());
                        resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());
                        RenderCommand::Dispatch(cmd,
                            {1'000'000, 1, 1}, {256, 1, 1});
                        /*RenderCommand::DispatchIndirect(cmd,
                            resources.GetBuffer(multiview->BatchDispatches[geometryIndex]),
                            batchIteration * sizeof(IndirectDispatchCommand));*/

                        MemoryDependencyInfo dependency = {
                            .SourceStage = PipelineStage::ComputeShader,
                            .DestinationStage = PipelineStage::ComputeShader,
                            .SourceAccess = PipelineAccess::WriteShader,
                            .DestinationAccess = PipelineAccess::ReadShader};
                        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                            .MemoryDependency(dependency)
                            .Build(frameContext.DeletionQueue));
                    }

                    /* prepare draws */
                    {
                        auto& pipeline = passData.PreparePipelines->at(batchIndex).Pipeline;
                        auto& resourceDescriptors = passData.PreparePipelines->at(batchIndex).ResourceDescriptors;
                        
                        auto& cmd = frameContext.Cmd;
                       
                        pipeline.BindCompute(cmd);
                        resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());
                        RenderCommand::PushConstants(cmd, pipeline.GetLayout(), multiview->TriangleViewCount);
                        RenderCommand::Dispatch(cmd,
                            {multiview->TriangleViewCount, 1, 1},
                            {MAX_CULL_VIEWS, 1, 1});

                        MemoryDependencyInfo dependency = {
                            .SourceStage = PipelineStage::ComputeShader,
                            .DestinationStage = PipelineStage::Indirect,
                            .SourceAccess = PipelineAccess::WriteShader,
                            .DestinationAccess = PipelineAccess::ReadIndirect};
                        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                            .MemoryDependency(dependency)
                            .Build(frameContext.DeletionQueue));
                    }
                
                    /* draw */
                    for (u32 i = 0; i < multiview->TriangleViewCount; i++)
                    {
                        auto& view = multiviewData->TriangleViews()[i];
                        auto&& [staticV, dynamicV] = view;
                        
                        auto& pipeline = passData.DrawPipelines->at(batchIndex).Pipelines[i];
                        auto& resourceDescriptors = passData.DrawPipelines->at(batchIndex).ResourceDescriptors[i];
                        // todo: can i bind less?
                        if (enumHasAny(staticV.DrawFeatures, Textures))
                        {
                            passData.DrawPipelines->at(0).ImmutableSamplerDescriptors[i].BindGraphicsImmutableSamplers(
                                frameContext.Cmd, pipeline.GetLayout());
                            passData.DrawPipelines->at(0).MaterialDescriptors[i].BindGraphics(frameContext.Cmd,
                                resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
                        }

                        auto& cmd = frameContext.Cmd;
                        
                        RenderCommand::BeginRendering(cmd, createRenderingInfo(batchIteration == 0, i));

                        RenderCommand::SetViewport(cmd, dynamicV.Resolution);
                        RenderCommand::SetScissors(cmd, {0, 0}, dynamicV.Resolution);
                        std::optional<DepthBias> depthBias = dynamicV.DrawInfo.Attachments.Depth.has_value() ?
                            dynamicV.DrawInfo.Attachments.Depth->DepthBias : std::nullopt;
                        if (depthBias.has_value())
                            RenderCommand::SetDepthBias(cmd, *depthBias);
                        
                        RenderCommand::BindIndexU32Buffer(cmd,
                            resources.GetBuffer(multiview->IndicesCulled[i][batchIndex]), 0);
                        pipeline.BindGraphics(cmd);
                        resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());

                        RenderCommand::DrawIndexedIndirect(cmd,
                            resources.GetBuffer(multiview->Draws[batchIndex]), i * sizeof(IndirectDrawCommand), 1);                    
                        
                        RenderCommand::EndRendering(cmd);
                    }
                    
                    signalBarrier(batchIndex);
                }
            }
        });
}
