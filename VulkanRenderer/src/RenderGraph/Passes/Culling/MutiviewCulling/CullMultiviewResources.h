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
    CullMultiviewResources createCullMultiview(CullMultiviewData& cullMultiviewData, Graph& graph);
    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph);
    
    template <typename BindGroup>
    void updateMeshCullMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview);
    
    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, Graph& graph);
    
    template <typename BindGroup>
    void updateCullMeshletMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage);

    CullTrianglesMultiviewResource createTriangleCullMultiview(CullMultiviewResources& multiview, Graph& graph);

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
        bindGroup.SetViewSpans({.Buffer = resources.GetBuffer(multiview.ViewSpans)});
        bindGroup.SetViews({.Buffer = resources.GetBuffer(multiview.Views)});

        for (u32 i = 0; i < multiview.GeometryCount; i++)
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(multiview.Objects[i])}, i);

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            auto&& [hiz, hizDescription] = resources.GetTextureWithDescription(multiview.HiZs[i]);

            bindGroup.SetHiz({.Image = hiz}, hizDescription.Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly, i);

            bindGroup.SetObjectVisibility({.Buffer = resources.GetBuffer(multiview.MeshVisibility[i])}, i);
        }
    }

    template <typename BindGroup>
    void updateCullMeshletMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage)
    {
        bindGroup.SetViewSpans({.Buffer = resources.GetBuffer(multiview.ViewSpans)});
        bindGroup.SetViews({.Buffer = resources.GetBuffer(multiview.Views)});

        Buffer countBuffer = resources.GetBuffer(cullStage == CullStage::Reocclusion ?
            multiview.CompactCommandCountReocclusion : multiview.CompactCommandCount);

        bindGroup.SetCount({.Buffer = countBuffer});
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(multiview.Objects[i])}, i);
            bindGroup.SetMeshlets({.Buffer = resources.GetBuffer(multiview.Meshlets[i])}, i);
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(multiview.Commands[i])}, i);
        }

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            auto&& [hiz, hizDescription] = resources.GetTextureWithDescription(multiview.HiZs[i]);

            bindGroup.SetHiz({.Image = hiz}, hizDescription.Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly, i);
            bindGroup.SetObjectVisibility({.Buffer = resources.GetBuffer(multiview.MeshVisibility[i])}, i);
            bindGroup.SetMeshletVisibility({.Buffer = resources.GetBuffer(multiview.MeshletVisibility[i])}, i);
            bindGroup.SetCompactedCommands({.Buffer = resources.GetBuffer(multiview.CompactCommands[i])}, i);
        }

        /* update `geometryCount` additional command and command count buffer bindings for triangle culling
         * we don't need it in `Reocclusion` stage
         */
        if (cullStage != CullStage::Reocclusion)
        {
            for (u32 i = 0; i < multiview.GeometryCount; i++)
            {
                u32 index = multiview.ViewCount + i;
                bindGroup.SetCompactedCommands({.Buffer = resources.GetBuffer(multiview.CompactCommands[index])},
                    index);
            }
        }
    }

    template <typename BindGroup>
    void updateCullTrianglePrepareMultiviewBindings(BindGroup& bindGroup, const Resources& resources,
        const CullTrianglesMultiviewResource& multiview)
    {
        bindGroup.SetCommandCounts({.Buffer = resources.GetBuffer(multiview.MeshletCull->CompactCommandCount)});
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
            bindGroup.SetDispatches({.Buffer = resources.GetBuffer(multiview.BatchDispatches[i])}, i);
    }

    template <typename CullBindGroup, typename PrepareBindGroup>
    void updateCullTriangleMultiviewBindings(CullBindGroup& cullBindGroup, const PrepareBindGroup& prepareBindGroup,
        const Resources& resources, const CullTrianglesMultiviewResource& multiview, u32 batchIndex)
    {
        /* update cull bindings */
        cullBindGroup.SetViewSpans({.Buffer = resources.GetBuffer(multiview.ViewSpans)});
        cullBindGroup.SetViews({.Buffer = resources.GetBuffer(multiview.Views)});

        cullBindGroup.SetCount({.Buffer = resources.GetBuffer(multiview.MeshletCull->CompactCommandCount)});
        
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
        {
            cullBindGroup.SetObjects({.Buffer = resources.GetBuffer(multiview.MeshletCull->Objects[i])}, i);
            cullBindGroup.SetPositions({.Buffer = resources.GetBuffer(multiview.AttributeBuffers[i].Positions)}, i);
            cullBindGroup.SetIndices({.Buffer = resources.GetBuffer(multiview.Indices[i])}, i);
        }

        for (u32 i = 0; i < multiview.TriangleViewCount; i++)
        {
            u32 meshletIndex = multiview.MeshletViewIndices[i];

            auto&& [hiz, hizDescription] = resources.GetTextureWithDescription(multiview.MeshletCull->HiZs[meshletIndex]);

            cullBindGroup.SetHiz({.Image = hiz}, hizDescription.Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly, i);
            cullBindGroup.SetMeshletVisibility({.Buffer = resources.GetBuffer(
                multiview.MeshletCull->MeshletVisibility[meshletIndex])}, i);

            cullBindGroup.SetTriangles({.Buffer = resources.GetBuffer(multiview.Triangles[i][batchIndex])}, i);
            cullBindGroup.SetTriangleVisibility({.Buffer = resources.GetBuffer(multiview.TriangleVisibility[i])}, i);
            cullBindGroup.SetCulledIndices({.Buffer = resources.GetBuffer(
                multiview.IndicesCulled[i][batchIndex])}, i);
        }
        cullBindGroup.SetCulledCount({.Buffer = resources.GetBuffer(multiview.IndicesCulledCount[batchIndex])});
        
        /* update `geometryCount` additional command and command count buffer bindings for triangle culling */
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
        {
            u32 index = multiview.MeshletCull->ViewCount + i;

            cullBindGroup.SetCommands({.Buffer = resources.GetBuffer(
                multiview.MeshletCull->CompactCommands[index])}, i);
        }
        
        /* update prepare bindings */
        prepareBindGroup.SetDraws({.Buffer = resources.GetBuffer(multiview.Draws[batchIndex])});
        prepareBindGroup.SetIndexCounts({.Buffer = resources.GetBuffer(multiview.IndicesCulledCount[batchIndex])});
    }
}
