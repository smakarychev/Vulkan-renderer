#include "CullMetaMultiviewPass.h"

#include "CullMultiviewUtils.h"
#include "MeshCullMultiviewPass.h"
#include "MeshletCullMultiviewPass.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/General/DrawIndirectCountPass.h"
#include "RenderGraph/Passes/HiZ/HiZFullPass.h"
#include "RenderGraph/Passes/HiZ/HiZNVPass.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"

namespace
{
    void ensureViewAttachments(std::string_view name, RG::Graph& renderGraph, CullViewDynamicDescription& view)
    {
        for (u32 i = 0; i < view.DrawInfo.Attachments.Colors.size(); i++)
        {
            auto& color = view.DrawInfo.Attachments.Colors[i];
            color.Resource = RG::RgUtils::ensureResource(color.Resource, renderGraph,
                std::format("{}.ColorIn.{}", name, i),
                RG::GraphTextureDescription{
                        .Width = view.Resolution.x,
                        .Height = view.Resolution.y,
                        .Format =  Format::RGBA16_FLOAT});
        }
    
        if (view.DrawInfo.Attachments.Depth.has_value())
        {
            auto& depth = *view.DrawInfo.Attachments.Depth;
            depth.Resource = RG::RgUtils::ensureResource(depth.Resource, renderGraph,
                std::format("{}.Depth", name),
                RG::GraphTextureDescription{
                    .Width = view.Resolution.x,
                    .Height = view.Resolution.y,
                    .Format =  Format::D32_FLOAT}); 
        }
    }

    void setAttachmentsLoadOperation(AttachmentLoad load, CullViewDynamicDescription& view)
    {
        for (u32 i = 0; i < view.DrawInfo.Attachments.Colors.size(); i++)
        {
            auto& color = view.DrawInfo.Attachments.Colors[i];
            color.Description.OnLoad = load;
        }
    
        if (view.DrawInfo.Attachments.Depth.has_value())
        {
            auto& depth = *view.DrawInfo.Attachments.Depth;
            depth.Description.OnLoad = load; 
        }
    }
}

RG::Pass& Passes::Meta::CullMultiview::addToGraph(std::string_view name, RG::Graph& renderGraph,
    CullMultiviewData& multiviewData)
{
    using namespace RG;

    struct MultiviewResources
    {
        CullMultiviewResources MultiviewResource{};
        CullTrianglesMultiviewResource MultiviewTrianglesResource{};
    };

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Meta.Setup")

            graph.HasSideEffect();
            passData.DrawAttachmentResources.resize(multiviewData.ViewCount());
            passData.HiZOut.resize(multiviewData.ViewCount());  
            
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                if (!view.Static.HiZContext ||
                    view.Static.HiZContext->GetDrawResolution().x != view.Dynamic.Resolution.x ||
                    view.Static.HiZContext->GetDrawResolution().y != view.Dynamic.Resolution.y ||
                    graph.ChangedResolution())
                {
                    multiviewData.UpdateViewHiZ(i,
                        std::make_shared<HiZPassContext>(
                            view.Dynamic.Resolution, graph.GetResolutionDeletionQueue()));
                }
            }

            MultiviewResources& resources = graph.GetOrCreateBlackboardValue<MultiviewResources>();
            resources.MultiviewResource = RgUtils::createCullMultiview(multiviewData, graph,
                std::string{name});
            resources.MultiviewTrianglesResource = RgUtils::createTriangleCullMultiview(resources.MultiviewResource,
                graph, std::string{name});
            std::unordered_map<Resource, Resource> attachmentRenames;
            resources.MultiviewTrianglesResource.AttachmentsRenames = &attachmentRenames;

            Multiview::MeshCull::addToGraph(std::format("{}.Mesh.Cull", name),
                graph, {.MultiviewResource = &resources.MultiviewResource}, CullStage::Cull);
            auto& meshletCull = Multiview::MeshletCull::addToGraph(std::format("{}.Meshlet.Cull", name),
                graph, {.MultiviewResource = &resources.MultiviewResource}, CullStage::Cull);
            auto& meshletCullOutput = graph.GetBlackboard().Get<Multiview::MeshletCull::PassData>(meshletCull);

            /* ensure all views have valid attachments */
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
                ensureViewAttachments(name, graph, multiviewData.View(i).Dynamic);

            /* draw views that do not use triangle culling */
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                
                if (view.Static.CullTriangles)
                    continue;
                
                Utils::updateRecordedAttachmentResources(view.Dynamic.DrawInfo.Attachments, attachmentRenames);

                auto& draw = Draw::IndirectCount::addToGraph(std::format("{}.Draw.Meshlet.{}", name, i), graph, {
                    .Geometry = view.Static.Geometry,
                    .Commands = meshletCullOutput.MultiviewResource->CompactCommands[i],
                    .CommandCount = meshletCullOutput.MultiviewResource->CompactCommandCount,
                    .CountOffset = i,
                    .Resolution = view.Dynamic.Resolution,
                    .Camera = view.Dynamic.Camera,
                    .DrawInfo = {
                        .Attachments = view.Dynamic.DrawInfo.Attachments,
                       // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
                        .IBL = view.Dynamic.DrawInfo.IBL,
                        .SSAO = view.Dynamic.DrawInfo.SSAO},
                    .Shader = view.Static.DrawShader});
                auto& drawOutput = graph.GetBlackboard().Get<Draw::IndirectCount::PassData>(draw);
                
                Utils::recordUpdatedAttachmentResources(view.Dynamic.DrawInfo.Attachments,
                    drawOutput.DrawAttachmentResources, attachmentRenames);
            }

            /* cull and draw views that do use triangle culling */
            if (multiviewData.TriangleViewCount() > 0)
            {
                Multiview::TrianglePrepareCull::addToGraph(std::format("{}.Triangle.Prepare.Cull", name),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource});
                Multiview::TriangleCull::addToGraph(std::format("{}.Triangle.Cull", name),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource}, CullStage::Cull);
            }

            /* update HiZs, now that all previously visible stuff was drawn */
            Pass* depthReduction = nullptr;
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                /* if we do not cull triangles, this is the last moment we can reduce depth */
                if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
                    multiviewData.IsPrimaryView(i) && !view.Static.CullTriangles ?
                        depthReduction = &HiZFull::addToGraph(std::format("{}.HiZFull", name), graph,
                            view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext) :
                        &HiZNV::addToGraph(std::format("{}.HiZ.{}", name, i), graph,
                            view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext);
            }

            /* update attachment on load operation */
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                setAttachmentsLoadOperation(AttachmentLoad::Load, multiviewData.View(i).Dynamic);

                if (i < multiviewData.TriangleViewCount())
                    setAttachmentsLoadOperation(AttachmentLoad::Load, multiviewData.TriangleView(i).Dynamic);
            }

            /* now we have to do triangle reocclusion */
            if (multiviewData.TriangleViewCount() > 0)
            {
                Multiview::TriangleCull::addToGraph(std::format("{}.Triangle.Reocclusion", name),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource}, CullStage::Reocclusion);
            }

            /* update HiZs with reoccluded triangles */
            for (u32 i = 0; i < multiviewData.TriangleViewCount(); i++)
            {
                auto& view = multiviewData.TriangleView(i);
                if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
                    multiviewData.IsPrimaryTriangleView(i) ?
                        depthReduction = &HiZFull::addToGraph(std::format("{}.HiZFull", name), graph,
                            view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext) :
                        &HiZNV::addToGraph(std::format("{}.HiZ.Reocclusion.{}", name, i), graph,
                            view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext);
            }

            /* finally, reocclude and draw meshlets for each view */
            Multiview::MeshCull::addToGraph(std::format("{}.Mesh.Reocclusion", name),
                graph, MeshCullMultiviewPassExecutionInfo{.MultiviewResource = &resources.MultiviewResource},
                CullStage::Reocclusion);
            auto& meshletReocclusion = Multiview::MeshletCull::addToGraph(
                std::format("{}.Meshlet.Reocclusion",name),
                graph, MeshletCullMultiviewPassExecutionInfo{.MultiviewResource = &resources.MultiviewResource},
                CullStage::Reocclusion);
            auto& meshletReocclusionOutput = graph.GetBlackboard().Get<Multiview::MeshletCull::PassData>(
                meshletReocclusion);

            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                Utils::updateRecordedAttachmentResources(
                    view.Dynamic.DrawInfo.Attachments, attachmentRenames);

                auto& draw = Draw::IndirectCount::addToGraph(std::format("{}.Draw.Meshlet.Reocclusion.{}", name, i),
                    graph, {
                        .Geometry = view.Static.Geometry,
                        .Commands = meshletReocclusionOutput.MultiviewResource->CompactCommands[i],
                        .CommandCount = meshletReocclusionOutput.MultiviewResource->CompactCommandCountReocclusion,
                        .CountOffset = i,
                        .Resolution = view.Dynamic.Resolution,
                        .Camera = view.Dynamic.Camera,
                        .DrawInfo = {
                            .Attachments = view.Dynamic.DrawInfo.Attachments,
                           // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
                            .IBL = view.Dynamic.DrawInfo.IBL,
                            .SSAO = view.Dynamic.DrawInfo.SSAO},
                        .Shader = view.Static.DrawShader});
                auto& drawOutput = graph.GetBlackboard().Get<Draw::IndirectCount::PassData>(draw);
                passData.DrawAttachmentResources[i] = drawOutput.DrawAttachmentResources;
                passData.HiZOut[i] = view.Static.HiZContext->GetHiZResource(HiZReductionMode::Min);
                                
                Utils::recordUpdatedAttachmentResources(
                    view.Dynamic.DrawInfo.Attachments, drawOutput.DrawAttachmentResources,
                    attachmentRenames);
            }
            if (depthReduction)
            {
                passData.HiZMaxOut = graph.GetBlackboard()
                    .Get<HiZFull::PassData>(*depthReduction).HiZMaxOut;
                passData.MinMaxDepth = graph.GetBlackboard()
                    .Get<HiZFull::PassData>(*depthReduction).MinMaxDepth;
            }

            multiviewData.NextFrame();

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}

