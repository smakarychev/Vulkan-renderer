#include "CullMetaMultiviewPass.h"

#include "CullMultiviewUtils.h"
#include "MeshCullMultiviewPass.h"
#include "MeshletCullMultiviewPass.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/General/DrawIndirectCountPass.h"
#include "RenderGraph/Passes/HiZ/HiZFullPass.h"
#include "RenderGraph/Passes/HiZ/HiZNVPass.h"

namespace
{
    void ensureViewAttachments(RG::Graph& renderGraph, CullViewDynamicDescription& view)
    {
        for (u32 i = 0; i < view.DrawInfo.Attachments.Colors.size(); i++)
        {
            auto& color = view.DrawInfo.Attachments.Colors[i];
            color.Resource = RG::RgUtils::ensureResource(color.Resource, renderGraph,
                StringId("ColorIn"_hsv).AddVersion(i),
                RG::GraphTextureDescription{
                        .Width = view.Resolution.x,
                        .Height = view.Resolution.y,
                        .Format =  Format::RGBA16_FLOAT});
        }
    
        if (view.DrawInfo.Attachments.Depth.has_value())
        {
            auto& depth = *view.DrawInfo.Attachments.Depth;
            depth.Resource = RG::RgUtils::ensureResource(depth.Resource, renderGraph,
                "Depth"_hsv,
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

RG::Pass& Passes::Meta::CullMultiview::addToGraph(StringId name, RG::Graph& renderGraph,
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
            
            MultiviewResources& resources = graph.GetOrCreateBlackboardValue<MultiviewResources>();
            resources.MultiviewResource = RgUtils::createCullMultiview(multiviewData, graph);
            resources.MultiviewTrianglesResource = RgUtils::createTriangleCullMultiview(resources.MultiviewResource,
                graph);
            std::unordered_map<Resource, Resource> attachmentRenames;
            resources.MultiviewTrianglesResource.AttachmentsRenames = &attachmentRenames;

            Multiview::MeshCull::addToGraph(name.Concatenate(".Mesh.Cull"),
                graph, {.MultiviewResource = &resources.MultiviewResource}, CullStage::Cull);
            auto& meshletCull = Multiview::MeshletCull::addToGraph(name.Concatenate(".Meshlet.Cull"),
                graph, {.MultiviewResource = &resources.MultiviewResource}, CullStage::Cull);
            auto& meshletCullOutput = graph.GetBlackboard().Get<Multiview::MeshletCull::PassData>(meshletCull);

            /* ensure all views have valid attachments */
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
                ensureViewAttachments(graph, multiviewData.View(i).Dynamic);

            /* draw views that do not use triangle culling */
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                
                if (view.Static.CullTriangles)
                    continue;
                
                Utils::updateRecordedAttachmentResources(view.Dynamic.DrawInfo.Attachments, attachmentRenames);

                auto& draw = Draw::IndirectCount::addToGraph(name.Concatenate(".Draw.Meshlet").AddVersion(i),
                    graph, {
                        .Geometry = view.Static.Geometry,
                        .Commands = meshletCullOutput.MultiviewResource->CompactCommands[i],
                        .CommandCount = meshletCullOutput.MultiviewResource->CompactCommandCount,
                        .CountOffset = i,
                        .Resolution = view.Dynamic.Resolution,
                        .Camera = view.Dynamic.Camera,
                        .DrawInfo = view.Dynamic.DrawInfo});
                auto& drawOutput = graph.GetBlackboard().Get<Draw::IndirectCount::PassData>(draw);
                
                Utils::recordUpdatedAttachmentResources(view.Dynamic.DrawInfo.Attachments,
                    drawOutput.DrawAttachmentResources, attachmentRenames);
            }

            /* cull and draw views that do use triangle culling */
            if (multiviewData.TriangleViewCount() > 0)
            {
                Multiview::TrianglePrepareCull::addToGraph(name.Concatenate(".Triangle.Prepare.Cull"),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource});
                Multiview::TriangleCull::addToGraph(name.Concatenate(".Triangle.Cull"),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource}, CullStage::Cull);
            }

            /* update HiZs, now that all previously visible stuff was drawn */
            Resource hiz{};
            Resource minMaxDepth{};
            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                /* if we do not cull triangles, this is the last moment we can reduce depth */
                if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
                {
                    if (multiviewData.IsPrimaryView(i) && !view.Static.CullTriangles)
                    {
                        auto& hizPass = HiZFull::addToGraph(name.Concatenate(".HiZFull"), graph, {
                            .Depth = view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            .Subresource = view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource});
                        auto& hizOutput = graph.GetBlackboard().Get<HiZFull::PassData>(hizPass);
                        hiz = hizOutput.HiZMin;
                        minMaxDepth = hizOutput.MinMaxDepth;
                        passData.HiZMaxOut = hizOutput.HiZMax;
                        passData.MinMaxDepth = minMaxDepth;
                    }
                    else
                    {
                        auto& hizPass = HiZNV::addToGraph(name.Concatenate(".HiZ.Reocclusion").AddVersion(i), graph, {
                            .Depth = view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            .Subresource = view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource,
                            .ReductionMode = HiZ::ReductionMode::Min});
                        auto& hizOutput = graph.GetBlackboard().Get<HiZNV::PassData>(hizPass);
                        hiz = hizOutput.HiZ;
                    }

                    resources.MultiviewResource.HiZs[i] = hiz;
                    passData.HiZOut[i] = hiz;
                }
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
                Multiview::TriangleCull::addToGraph(name.Concatenate(".Triangle.Reocclusion"),
                    graph, {.MultiviewResource = &resources.MultiviewTrianglesResource}, CullStage::Reocclusion);
            }

            /* update HiZs with reoccluded triangles */
            for (u32 i = 0; i < multiviewData.TriangleViewCount(); i++)
            {
                auto& view = multiviewData.TriangleView(i);
                if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
                {
                    if (multiviewData.IsPrimaryTriangleView(i))
                    {
                        auto& hizPass = HiZFull::addToGraph(name.Concatenate(".HiZFull"), graph, {
                            .Depth = view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            .Subresource = view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource});
                        auto& hizOutput = graph.GetBlackboard().Get<HiZFull::PassData>(hizPass);
                        hiz = hizOutput.HiZMin;
                        minMaxDepth = hizOutput.MinMaxDepth;

                        passData.HiZMaxOut = hizOutput.HiZMax;
                        passData.MinMaxDepth = minMaxDepth;
                    }
                    else
                    {
                        auto& hizPass = HiZNV::addToGraph(name.Concatenate(".HiZ.Reocclusion").AddVersion(i), graph, {
                            .Depth = view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                            .Subresource = view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource,
                            .ReductionMode = HiZ::ReductionMode::Min});
                        auto& hizOutput = graph.GetBlackboard().Get<HiZNV::PassData>(hizPass);
                        hiz = hizOutput.HiZ;
                    }
                    
                    passData.HiZOut[i] = hiz;
                    resources.MultiviewResource.HiZs[i] = hiz;
                }
            }

            /* finally, reocclude and draw meshlets for each view */
            Multiview::MeshCull::addToGraph(name.Concatenate(".Mesh.Reocclusion"),
                graph, MeshCullMultiviewPassExecutionInfo{.MultiviewResource = &resources.MultiviewResource},
                CullStage::Reocclusion);
            auto& meshletReocclusion = Multiview::MeshletCull::addToGraph(
                name.Concatenate(".Meshlet.Reocclusion"),
                graph, MeshletCullMultiviewPassExecutionInfo{.MultiviewResource = &resources.MultiviewResource},
                CullStage::Reocclusion);
            auto& meshletReocclusionOutput = graph.GetBlackboard().Get<Multiview::MeshletCull::PassData>(
                meshletReocclusion);

            for (u32 i = 0; i < multiviewData.ViewCount(); i++)
            {
                auto& view = multiviewData.View(i);
                Utils::updateRecordedAttachmentResources(
                    view.Dynamic.DrawInfo.Attachments, attachmentRenames);

                auto& draw = Draw::IndirectCount::addToGraph(
                    name.Concatenate(".Draw.Meshlet.Reocclusion").AddVersion(i),
                    graph, {
                        .Geometry = view.Static.Geometry,
                        .Commands = meshletReocclusionOutput.MultiviewResource->CompactCommands[i],
                        .CommandCount = meshletReocclusionOutput.MultiviewResource->CompactCommandCountReocclusion,
                        .CountOffset = i,
                        .Resolution = view.Dynamic.Resolution,
                        .Camera = view.Dynamic.Camera,
                        .DrawInfo = view.Dynamic.DrawInfo});
                auto& drawOutput = graph.GetBlackboard().Get<Draw::IndirectCount::PassData>(draw);
                passData.DrawAttachmentResources[i] = drawOutput.DrawAttachmentResources;
                passData.HiZOut[i] = resources.MultiviewResource.HiZs[i];
                                
                Utils::recordUpdatedAttachmentResources(
                    view.Dynamic.DrawInfo.Attachments, drawOutput.DrawAttachmentResources,
                    attachmentRenames);
            }

            multiviewData.NextFrame();

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}

