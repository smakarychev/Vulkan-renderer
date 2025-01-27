#pragma once

#include "CullMultiviewData.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"
#include "RenderGraph/RenderGraph.h"

#include <vector>

#include "CullMultiviewUtils.h"

struct TriangleDrawContextMultiview;
class CullMultiviewData;

namespace RG
{
    struct CullMultiviewResources
    {
        u32 ViewCount{0};
        u32 GeometryCount{0};
        
        Resource ViewSpans;
        Resource Views;
        Sampler HiZSampler{};
        
        std::vector<Resource> Objects;
        std::vector<Resource> Meshlets;
        std::vector<Resource> Commands;
        
        std::vector<Resource> HiZs;
        
        std::vector<Resource> MeshVisibility;
        std::vector<Resource> MeshletVisibility;

        std::vector<Resource> CompactCommands;
        Resource CompactCommandCount;
        Resource CompactCommandCountReocclusion;

        CullMultiviewData* Multiview{nullptr};
    };

    struct CullTrianglesMultiviewResource
    {
        u32 MaxDispatches{0};
        u32 TriangleViewCount{0};
        std::vector<u32> MeshletViewIndices;

        Resource ViewSpans;
        Resource Views;

        std::vector<Resource> Indices;
        std::vector<Resource> BatchDispatches;
        
        std::vector<Resource> TriangleVisibility;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Triangles;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulled;
        std::array<Resource, BATCH_OVERLAP> IndicesCulledCount;
        std::array<Resource, BATCH_OVERLAP> Draws;

        std::vector<DrawAttributeBuffers> AttributeBuffers;
        std::vector<Resource> Cameras;
        std::vector<SceneLightResources> SceneLights;
        std::vector<DrawAttachmentResources> AttachmentResources;
        std::vector<IBLData> IBLs;
        std::vector<SSAOData> SSAOs;
        std::vector<CSMData> CSMs;
        
        CullMultiviewResources* MeshletCull{nullptr};
        CullMultiviewData* Multiview{nullptr};
        Utils::AttachmentsRenames* AttachmentsRenames{nullptr};
    };
}

namespace RG::RgUtils
{
    CullMultiviewResources createCullMultiview(CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName);
    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph);
    
    template <typename BindGroup>
    void updateMeshCullMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview);
    
    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, Graph& graph);
    
    template <typename BindGroup>
    void updateCullMeshletMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage);

    CullTrianglesMultiviewResource createTriangleCullMultiview(CullMultiviewResources& multiview, Graph& graph,
        const std::string& baseName);

    void readWriteCullTrianglePrepareMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph);
    
    template <typename BindGroup>
    void updateCullTrianglePrepareMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullTrianglesMultiviewResource& multiview);

    void readWriteCullTriangleMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph);
    
    template <typename CullBindGroup, typename PrepareBindGroup>
    void updateCullTriangleMultiviewBindings(CullBindGroup& cullBindGroup,
        const PrepareBindGroup& prepareBindGroup,
        const Resources& resources,
        const CullTrianglesMultiviewResource& multiview,
        u32 batchIndex);

    template <typename BindGroup>
    void updateMeshCullMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview)
    {
        bindGroup.SetViewSpans(resources.GetBuffer(multiview.ViewSpans).BindingInfo());
        bindGroup.SetViews(resources.GetBuffer(multiview.Views).BindingInfo());

        for (u32 i = 0; i < multiview.GeometryCount; i++)
            bindGroup.SetObjects(resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);

            bindGroup.SetHiz(hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);

            bindGroup.SetObjectVisibility(resources.GetBuffer(multiview.MeshVisibility[i]) .BindingInfo(), i);
        }
    }

    template <typename BindGroup>
    void updateCullMeshletMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage)
    {
        bindGroup.SetViewSpans(resources.GetBuffer(multiview.ViewSpans).BindingInfo());
        bindGroup.SetViews(resources.GetBuffer(multiview.Views).BindingInfo());

        const Buffer& countBuffer = resources.GetBuffer(cullStage == CullStage::Reocclusion ?
            multiview.CompactCommandCountReocclusion : multiview.CompactCommandCount);

        bindGroup.SetCount(countBuffer.BindingInfo());
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            bindGroup.SetObjects(resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);
            bindGroup.SetMeshlets(resources.GetBuffer(multiview.Meshlets[i]).BindingInfo(), i);
            bindGroup.SetCommands(resources.GetBuffer(multiview.Commands[i]).BindingInfo(), i);
        }

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);

            bindGroup.SetHiz(hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);

            bindGroup.SetObjectVisibility(resources.GetBuffer(multiview.MeshVisibility[i]).BindingInfo(), i);
            bindGroup.SetMeshletVisibility(resources.GetBuffer(multiview.MeshletVisibility[i]).BindingInfo(), i);
            bindGroup.SetCompactedCommands(resources.GetBuffer(multiview.CompactCommands[i]).BindingInfo(), i);
        }

        /* update `geometryCount` additional command and command count buffer bindings for triangle culling
         * we don't need it in `Reocclusion` stage
         */
        if (cullStage != CullStage::Reocclusion)
        {
            for (u32 i = 0; i < multiview.GeometryCount; i++)
            {
                u32 index = multiview.ViewCount + i;
                bindGroup.SetCompactedCommands(resources.GetBuffer(multiview.CompactCommands[index])
                    .BindingInfo(), index);
            }
        }
    }

    template <typename BindGroup>
    void updateCullTrianglePrepareMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullTrianglesMultiviewResource& multiview)
    {
        bindGroup.SetCommandCounts(resources.GetBuffer(multiview.MeshletCull->CompactCommandCount).BindingInfo());
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
            bindGroup.SetDispatches(resources.GetBuffer(multiview.BatchDispatches[i]).BindingInfo(), i);
    }

    template <typename CullBindGroup, typename PrepareBindGroup>
    void updateCullTriangleMultiviewBindings(CullBindGroup& cullBindGroup, const PrepareBindGroup& prepareBindGroup,
        const Resources& resources, const CullTrianglesMultiviewResource& multiview, u32 batchIndex)
    {
        /* update cull bindings */
        cullBindGroup.SetViewSpans(resources.GetBuffer(multiview.ViewSpans).BindingInfo());
        cullBindGroup.SetViews(resources.GetBuffer(multiview.Views).BindingInfo());

        cullBindGroup.SetCount(resources.GetBuffer(multiview.MeshletCull->CompactCommandCount).BindingInfo());
        
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
        {
            cullBindGroup.SetObjects(resources.GetBuffer(multiview.MeshletCull->Objects[i]).BindingInfo(), i);
            cullBindGroup.SetPositions(resources.GetBuffer(multiview.AttributeBuffers[i].Positions).BindingInfo(), i);
            cullBindGroup.SetIndices(resources.GetBuffer(multiview.Indices[i]).BindingInfo(), i);
        }

        for (u32 i = 0; i < multiview.TriangleViewCount; i++)
        {
            u32 meshletIndex = multiview.MeshletViewIndices[i];
            
            const Texture& hiz = resources.GetTexture(multiview.MeshletCull->HiZs[meshletIndex]);
            cullBindGroup.SetHiz(hiz.BindingInfo(multiview.MeshletCull->HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);

            cullBindGroup.SetMeshletVisibility(resources.GetBuffer(
                multiview.MeshletCull->MeshletVisibility[meshletIndex]).BindingInfo(), i);

            cullBindGroup.SetTriangles(resources.GetBuffer(multiview.Triangles[i][batchIndex]).BindingInfo(), i);
            cullBindGroup.SetTriangleVisibility(resources.GetBuffer(multiview.TriangleVisibility[i]).BindingInfo(), i);
            cullBindGroup.SetCulledIndices(resources.GetBuffer(
                multiview.IndicesCulled[i][batchIndex]).BindingInfo(), i);
        }
        cullBindGroup.SetCulledCount(resources.GetBuffer(multiview.IndicesCulledCount[batchIndex]).BindingInfo());
        
        /* update `geometryCount` additional command and command count buffer bindings for triangle culling */
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
        {
            u32 index = multiview.MeshletCull->ViewCount + i;

            cullBindGroup.SetCommands(resources.GetBuffer(
                multiview.MeshletCull->CompactCommands[index]).BindingInfo(), i);
        }
        
        /* update prepare bindings */
        prepareBindGroup.SetDraws(resources.GetBuffer(multiview.Draws[batchIndex]).BindingInfo());
        prepareBindGroup.SetIndexCounts(resources.GetBuffer(multiview.IndicesCulledCount[batchIndex]).BindingInfo());
    }
}
