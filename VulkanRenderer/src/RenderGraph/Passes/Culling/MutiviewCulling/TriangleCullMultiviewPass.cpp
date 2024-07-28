#include "TriangleCullMultiviewPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "Rendering/ShaderCache.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Multiview::TrianglePrepareCull::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const TriangleCullPrepareMultiviewPassExecutionInfo& info)
{
    using namespace RG;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            graph.SetShader("../assets/shaders/prepare-dispatches-multiview.shader");
            
            RgUtils::readWriteCullTrianglePrepareMultiview(*info.MultiviewResource, renderGraph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            GPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            
            auto* multiview = passData.MultiviewResource;

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline();
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

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

    return pass;
}

RG::Pass& Passes::Multiview::TriangleCull::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const TriangleCullMultiviewPassExecutionInfo& info, CullStage stage)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct Barriers
    {
        std::array<SplitBarrier, TriangleCullMultiviewTraits::MAX_BATCHES> SplitBarriers;
        DependencyInfo SplitBarrierDependency;
    };
    struct PassDataPrivate
    {
        CullTrianglesMultiviewResource* MultiviewResource{nullptr};
        std::vector<DrawExecutionInfo> TriangleDrawInfos;
    };

    std::string passName = std::string{name};

    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Triangle.Cull.Draw.Multiview.Setup")

            for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
            {
                ShaderCache::Register(std::format("{}.{}", name, i),
                    "../assets/shaders/triangle-cull-multiview.shader",
                    ShaderOverrides{}
                        .Add({"REOCCLUSION"}, stage == CullStage::Reocclusion)
                        .Add({"SINGLE_PASS"}, stage == CullStage::Single));
                ShaderCache::Register(std::format("{}.PrepareDraw.{}", name, i),
                        "../assets/shaders/prepare-draws-multiview.shader", {});

                for (u32 view = 0; view < info.MultiviewResource->TriangleViewCount; view++)
                {
                    ShaderCache::Register(std::format("{}.Draw.{}.{}", name, i, view),
                        info.MultiviewResource->Multiview->TriangleView(view).Static.DrawTrianglesShader,
                        info.MultiviewResource->Multiview->TriangleView(view).Static.DrawTrianglesShader->CopyOverrides());    
                }
            }
            

            if (!graph.TryGetBlackboardValue<Barriers>())
            {
                Barriers barriers = {};
                for (auto& splitBarrier : barriers.SplitBarriers)
                    splitBarrier = SplitBarrier::Builder().Build();
                barriers.SplitBarrierDependency = DependencyInfo::Builder()
                    .MemoryDependency({
                        .SourceStage = PipelineStage::ComputeShader,
                        .DestinationStage = PipelineStage::PixelShader,
                        .SourceAccess = PipelineAccess::WriteShader,
                        .DestinationAccess = PipelineAccess::ReadStorage})
                    .Build();
                graph.UpdateBlackboard(barriers);
            }
            
            if (stage != CullStage::Cull)
            {
                for (u32 i = 0; i < info.MultiviewResource->TriangleViewCount; i++)
                {
                    u32 meshletIndex = info.MultiviewResource->MeshletViewIndices[i];
                    info.MultiviewResource->MeshletCull->HiZs[meshletIndex] =
                        info.MultiviewResource->MeshletCull->Multiview->View(meshletIndex)
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
            for (u32 i = 0; i < info.MultiviewResource->Multiview->TriangleViewCount(); i++)
                passData.TriangleDrawInfos.push_back(
                    info.MultiviewResource->Multiview->TriangleView(i).Dynamic.DrawInfo);

            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources.reserve(info.MultiviewResource->TriangleViewCount);
            for (u32 i = 0; i < info.MultiviewResource->TriangleViewCount; i++)
                passDataPublic.DrawAttachmentResources.push_back(info.MultiviewResource->AttachmentResources[i]);
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Triangle.Cull.Draw.Multiview")
            GPU_PROFILE_FRAME("Triangle.Cull.Draw.Multiview")

            using enum DrawFeatures;

            auto* multiview = passData.MultiviewResource;
            auto* multiviewData = multiview->MeshletCull->Multiview;

            auto& barriers = resources.GetOrCreateValue<Barriers>();

            auto createRenderingInfo = [&](bool canClear, u32 viewIndex)
            {
                auto& view = multiviewData->TriangleView(viewIndex);
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

                barriers.SplitBarriers[batchIndex].Wait(frameContext.Cmd, barriers.SplitBarrierDependency);
                barriers.SplitBarriers[batchIndex].Reset(frameContext.Cmd, barriers.SplitBarrierDependency);
            };
            auto signalBarrier = [&](u32 batchIndex)
            {
                barriers.SplitBarriers[batchIndex].Signal(frameContext.Cmd, barriers.SplitBarrierDependency);
            };

            resources.GetBuffer(multiview->ViewSpans, multiviewData->TriangleViewSpans().data(),
                multiviewData->TriangleViewSpans().size() * sizeof(CullMultiviewData::ViewSpan), 0,
                *frameContext.ResourceUploader);
            std::vector<CullViewDataGPU> views = multiviewData->CreateMultiviewGPUTriangles();
                resources.GetBuffer(multiview->Views, views.data(), views.size() * sizeof(CullViewDataGPU), 0,
                    *frameContext.ResourceUploader);
            for (u32 i = 0; i < multiview->TriangleViewCount; i++)
            {
                auto& view = multiviewData->TriangleView(i);
                CameraGPU cameraGPU = CameraGPU::FromCamera(*view.Dynamic.Camera, view.Dynamic.Resolution);
                resources.GetBuffer(multiview->Cameras[i], cameraGPU, *frameContext.ResourceUploader);

                for (u32 batchIndex = 0; batchIndex < TriangleCullMultiviewTraits::MAX_BATCHES; batchIndex++)
                    resources.GetBuffer(multiview->IndicesCulledCount[batchIndex], 0, i * sizeof(u32),
                        *frameContext.ResourceUploader);
            }

            /* update all bindings */
            for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
            {
                auto& cullShader = ShaderCache::Get(std::format("{}.{}", passName, i));
                auto& prepareShader = ShaderCache::Get(std::format("{}.PrepareDraw.{}", passName, i));

                auto& cullSamplerDescriptors = cullShader.Descriptors(ShaderDescriptorsKind::Sampler);
                Sampler hizSampler = multiview->MeshletCull->HiZSampler;
                cullSamplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                    multiview->MeshletCull->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

                std::vector<ShaderDescriptors> drawDescriptors;
                drawDescriptors.reserve(multiview->TriangleViewCount);
                for (u32 view = 0; view < multiview->TriangleViewCount; view++)
                    drawDescriptors.push_back(
                        ShaderCache::Get(std::format("{}.Draw.{}.{}", passName, i, view))
                            .Descriptors(ShaderDescriptorsKind::Resource));
                
                RgUtils::updateCullTriangleMultiviewBindings(
                    cullShader.Descriptors(ShaderDescriptorsKind::Resource),
                    prepareShader.Descriptors(ShaderDescriptorsKind::Resource),
                    drawDescriptors,
                    resources, *multiview, i);
            }

            /* if there are no batches, we clear the render target (if needed) */
            if (multiviewData->GetBatchIterationCount() == 0)
            {
                auto& cmd = frameContext.Cmd;

                for (u32 i = 0; i < multiview->TriangleViewCount; i++)
                {
                    u32 meshletIndex = multiview->MeshletViewIndices[i];
                    auto& view = multiviewData->View(meshletIndex);

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
                        auto& cullShader = ShaderCache::Get(std::format("{}.{}", passName, batchIndex));
                        auto& pipeline = cullShader.Pipeline();
                        auto& samplerDescriptors = cullShader.Descriptors(ShaderDescriptorsKind::Sampler);
                        auto& resourceDescriptors = cullShader.Descriptors(ShaderDescriptorsKind::Resource);

                        struct PushConstants
                        {
                            u32 CommandOffset;
                            u32 MaxCommandIndex;
                            u32 GeometryIndex;
                            u32 MeshletViewCount;
                        };
                        PushConstants pushConstants = {
                            .CommandOffset = batchIteration * TriangleCullMultiviewTraits::CommandCount(),
                            .MaxCommandIndex = TriangleCullMultiviewTraits::CommandCount(),
                            .GeometryIndex = geometryIndex,
                            .MeshletViewCount = multiview->MeshletCull->ViewCount};
                        auto& cmd = frameContext.Cmd;
                        pipeline.BindCompute(cmd);
                        RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
                        samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());
                        resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                            pipeline.GetLayout());
                        RenderCommand::DispatchIndirect(cmd,
                            resources.GetBuffer(multiview->BatchDispatches[geometryIndex]),
                            batchIteration * sizeof(IndirectDispatchCommand));

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
                        auto& prepareShader = ShaderCache::Get(std::format("{}.PrepareDraw.{}", passName, batchIndex));
                        auto& pipeline = prepareShader.Pipeline();
                        auto& resourceDescriptors = prepareShader.Descriptors(ShaderDescriptorsKind::Resource);
                        
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
                        auto& view = multiviewData->TriangleView(i);
                        auto&& [staticV, dynamicV] = view;

                        auto& drawShader = ShaderCache::Get(std::format("{}.Draw.{}.{}", passName, batchIndex, i));
                        auto& pipeline = drawShader.Pipeline();
                        auto& resourceDescriptors = drawShader.Descriptors(ShaderDescriptorsKind::Resource);
                        // todo: can i bind less?
                        if (enumHasAny(drawShader.Features(), Textures))
                        {
                            drawShader.Descriptors(ShaderDescriptorsKind::Sampler)
                                .BindGraphicsImmutableSamplers(frameContext.Cmd, pipeline.GetLayout());
                            drawShader.Descriptors(ShaderDescriptorsKind::Materials)
                                .BindGraphics(frameContext.Cmd,
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

    return pass;
}