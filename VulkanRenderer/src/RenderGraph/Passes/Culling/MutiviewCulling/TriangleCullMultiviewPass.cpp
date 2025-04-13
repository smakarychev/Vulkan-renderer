#include "TriangleCullMultiviewPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/Passes/Generated/PrepareDispatchesMultiviewBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/PrepareDrawsMultiviewBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/TriangleCullMultiviewBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"

RG::Pass& Passes::Multiview::TrianglePrepareCull::addToGraph(StringId name, RG::Graph& renderGraph,
    const TriangleCullPrepareMultiviewPassExecutionInfo& info)
{
    using namespace RG;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            graph.SetShader("prepare-dispatches-multiview.shader");
            
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
            PrepareDispatchesMultiviewShaderBindGroup bindGroup(shader);

            RgUtils::updateCullTrianglePrepareMultiviewBindings(bindGroup, resources, *multiview);
            struct PushConstants
            {
                u32 CommandsPerBatchCount;
                u32 CommandsMultiplier;
                u32 LocalGroupX;
                u32 GeometryCount;
                u32 MeshletViewCount;
                u32 MaxDispatches;
            };

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());

            PushConstants pushConstants = {
                .CommandsPerBatchCount = TriangleCullMultiviewTraits::CommandCount(),
                .CommandsMultiplier = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .LocalGroupX = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .GeometryCount = multiview->MeshletCull->GeometryCount,
                .MeshletViewCount = multiview->MeshletCull->ViewCount,
                .MaxDispatches = multiview->MaxDispatches};
            
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            
            cmd.Dispatch({
                .Invocations = {multiview->MaxDispatches, 1, 1},
                .GroupSize = {64, 1, 1}});

            multiview->MeshletCull->Multiview->UpdateBatchIterationCount();
        });

    return pass;
}

RG::Pass& Passes::Multiview::TriangleCull::addToGraph(StringId name, RG::Graph& renderGraph,
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

    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Triangle.Cull.Draw.Multiview.Setup")

            for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
            {
                ShaderCache::Register(name.AddVersion(i),
                    "triangle-cull-multiview.shader",
                    ShaderOverrides{
                        ShaderOverride{"REOCCLUSION"_hsv, stage == CullStage::Reocclusion},
                        ShaderOverride{"SINGLE_PASS"_hsv, stage == CullStage::Single}});
                ShaderCache::Register(StringId("{}.PrepareDraw.{}", name, i),
                    "prepare-draws-multiview.shader", {});
            }

            if (!graph.TryGetBlackboardValue<Barriers>())
            {
                Barriers barriers = {};
                for (auto& splitBarrier : barriers.SplitBarriers)
                    splitBarrier = Device::CreateSplitBarrier();
                barriers.SplitBarrierDependency = Device::CreateDependencyInfo({
                    .MemoryDependencyInfo = MemoryDependencyInfo{
                        .SourceStage = PipelineStage::ComputeShader,
                        .DestinationStage = PipelineStage::PixelShader,
                        .SourceAccess = PipelineAccess::WriteShader,
                        .DestinationAccess = PipelineAccess::ReadStorage}});
                graph.UpdateBlackboard(barriers);
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

            auto* multiview = passData.MultiviewResource;
            auto* multiviewData = multiview->MeshletCull->Multiview;

            auto& barriers = resources.GetOrCreateValue<Barriers>();

            auto createRenderingInfo = [&](bool canClear, u32 viewIndex)
            {
                auto& view = multiviewData->TriangleView(viewIndex);
                auto&& [staticV, dynamicV] = view;

                auto& colors = passData.TriangleDrawInfos[viewIndex].Attachments.Colors;
                auto& depth = passData.TriangleDrawInfos[viewIndex].Attachments.Depth;
                
                std::vector<RenderingAttachment> colorAttachments;
                colorAttachments.reserve(colors.size());
                std::optional<RenderingAttachment> depthAttachment;
                for (u32 attachmentIndex = 0; attachmentIndex < colors.size(); attachmentIndex++)
                {
                    auto description = colors[attachmentIndex].Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    Texture colorTexture = resources.GetTexture(colors[attachmentIndex].Resource);
                    colorAttachments.push_back(Device::CreateRenderingAttachment({
                        .Description = description,
                        .Image = colorTexture,
                        .Layout = ImageLayout::Attachment},
                        frameContext.DeletionQueue));
                }
                if (depth.has_value())
                {
                    auto description = depth->Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    Texture depthTexture = resources.GetTexture(depth->Resource);
                    depthAttachment = Device::CreateRenderingAttachment({
                        .Description = description,
                        .Image = depthTexture,
                        .Layout = ImageLayout::DepthAttachment},
                        frameContext.DeletionQueue);
                }
                
                return Device::CreateRenderingInfo({
                    .RenderArea = dynamicV.Resolution,
                    .ColorAttachments = colorAttachments,
                    .DepthAttachment = depthAttachment},
                    frameContext.DeletionQueue);
            };
            auto waitOnBarrier = [&](u32 batchIndex, u32 index)
            {
                if (index < TriangleCullMultiviewTraits::MAX_BATCHES)
                    return;

                frameContext.CommandList.WaitOnSplitBarrier({
                    .SplitBarrier = barriers.SplitBarriers[batchIndex],
                    .DependencyInfo = barriers.SplitBarrierDependency});
                frameContext.CommandList.ResetSplitBarrier({
                    .SplitBarrier = barriers.SplitBarriers[batchIndex],
                    .DependencyInfo = barriers.SplitBarrierDependency});
            };
            auto signalBarrier = [&](u32 batchIndex)
            {
                frameContext.CommandList.SignalSplitBarrier({
                    .SplitBarrier = barriers.SplitBarriers[batchIndex],
                    .DependencyInfo = barriers.SplitBarrierDependency});
            };

            /* update all bindings */
            for (u32 batchIndex = 0; batchIndex < TriangleCullMultiviewTraits::MAX_BATCHES; batchIndex++)
            {
                Sampler hizSampler = multiview->MeshletCull->HiZSampler;
                
                const Shader& cullShader = ShaderCache::Get(name.AddVersion(batchIndex));
                const Shader& prepareShader = ShaderCache::Get(StringId("{}.PrepareDraw.{}", name, batchIndex));
                
                TriangleCullMultiviewShaderBindGroup cullBindGroup(cullShader);
                PrepareDrawsMultiviewShaderBindGroup prepareBindGroup(prepareShader);

                cullBindGroup.SetSampler(hizSampler);

                RgUtils::updateCullTriangleMultiviewBindings(
                    cullBindGroup,
                    prepareBindGroup,
                    resources, *multiview, batchIndex);
            }

            /* if there are no batches, we clear the render target (if needed) */
            if (multiviewData->GetBatchIterationCount() == 0)
            {
                auto& cmd = frameContext.CommandList;

                for (u32 i = 0; i < multiview->TriangleViewCount; i++)
                {
                    u32 meshletIndex = multiview->MeshletViewIndices[i];
                    auto& view = multiviewData->View(meshletIndex);

                    cmd.BeginRendering({.RenderingInfo = createRenderingInfo(true, i)});
                    cmd.SetViewport({.Size = view.Dynamic.Resolution});
                    cmd.SetScissors({.Size = view.Dynamic.Resolution});
                    cmd.EndRendering({});
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
                        const Shader& shader = ShaderCache::Get(name.AddVersion(batchIndex));
                        TriangleCullMultiviewShaderBindGroup bindGroup(shader);
                        
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
                        auto& cmd = frameContext.CommandList;
                        bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                        cmd.PushConstants({
                            .PipelineLayout = shader.GetLayout(), 
                            .Data = {pushConstants}});
                        cmd.DispatchIndirect({
                            .Buffer = resources.GetBuffer(multiview->BatchDispatches[geometryIndex]),
                            .Offset = batchIteration * sizeof(IndirectDispatchCommand)});

                        cmd.WaitOnBarrier({
                            .DependencyInfo = Device::CreateDependencyInfo({
                                .MemoryDependencyInfo = MemoryDependencyInfo {
                                .SourceStage = PipelineStage::ComputeShader,
                                .DestinationStage = PipelineStage::ComputeShader,
                                .SourceAccess = PipelineAccess::WriteShader,
                                .DestinationAccess = PipelineAccess::ReadShader}},
                                frameContext.DeletionQueue)});
                    }

                    /* prepare draws */
                    {
                        const Shader& shader = ShaderCache::Get(StringId("{}.PrepareDraw.{}", name, batchIndex));
                        PrepareDrawsMultiviewShaderBindGroup bindGroup(shader);                        
                        auto& cmd = frameContext.CommandList;
                        bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                        cmd.PushConstants({
                            .PipelineLayout = shader.GetLayout(), 
                            .Data = {multiview->TriangleViewCount}});
                        cmd.Dispatch({
                            .Invocations = {multiview->TriangleViewCount, 1, 1},
                            .GroupSize = {MAX_CULL_VIEWS, 1, 1}});

                        DependencyInfo dependencyInfo = Device::CreateDependencyInfo({
                            .MemoryDependencyInfo = MemoryDependencyInfo{
                            .SourceStage = PipelineStage::ComputeShader,
                            .DestinationStage = PipelineStage::Indirect,
                            .SourceAccess = PipelineAccess::WriteShader,
                            .DestinationAccess = PipelineAccess::ReadIndirect}},
                            frameContext.DeletionQueue);
                        cmd.WaitOnBarrier({
                            .DependencyInfo = dependencyInfo});
                    }
                
                    /* draw */
                    for (u32 i = 0; i < multiview->TriangleViewCount; i++)
                    {
                        // todo: to callback
                        auto& view = multiviewData->TriangleView(i);
                        auto&& [staticV, dynamicV] = view;

                        auto& cmd = frameContext.CommandList;

                        cmd.BeginRendering({
                            .RenderingInfo = createRenderingInfo(batchIteration == 0, i)});

                        cmd.SetViewport({.Size = dynamicV.Resolution});
                        cmd.SetScissors({.Size = dynamicV.Resolution});
                        std::optional<DepthBias> depthBias = dynamicV.DrawInfo.Attachments.Depth.has_value() ?
                            dynamicV.DrawInfo.Attachments.Depth->DepthBias : std::nullopt;
                        if (depthBias.has_value())
                            cmd.SetDepthBias({
                                .Constant = depthBias->Constant, .Slope = depthBias->Slope});

                        cmd.BindIndexU32Buffer({
                            .Buffer = resources.GetBuffer(multiview->IndicesCulled[i][batchIndex])});

                        dynamicV.DrawInfo.DrawBind(frameContext.CommandList, resources, {
                            .Camera = multiview->Cameras[i],
                            .Objects = multiview->MeshletCull->Objects[geometryIndex],
                            .Commands = multiview->MeshletCull->Commands[geometryIndex],
                            .DrawAttributes = multiview->AttributeBuffers[geometryIndex],
                            .Triangles = multiview->Triangles[i][batchIndex],
                            .ExecutionId = batchIndex});

                        
                        cmd.DrawIndexedIndirect({
                            .Buffer = resources.GetBuffer(multiview->Draws[batchIndex]),
                            .Offset = i * sizeof(IndirectDrawCommand),
                            .Count = 1});           

                        cmd.EndRendering({});
                    }
                    
                    signalBarrier(batchIndex);
                }
            }
        });

    return pass;
}