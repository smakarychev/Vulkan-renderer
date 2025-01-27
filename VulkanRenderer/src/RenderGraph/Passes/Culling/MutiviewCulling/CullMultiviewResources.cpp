#include "CullMultiviewResources.h"

#include <ranges>

#include "CameraGPU.h"
#include "CullMultiviewData.h"
#include "RenderGraph/RGUtils.h"
#include "Scene/SceneGeometry.h"

namespace RG::RgUtils
{
    CullMultiviewResources createCullMultiview(CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName)
    {
        CullMultiviewResources multiviewResource = {};
        
        multiviewResource.Multiview = &cullMultiviewData;
        u32 viewCount = cullMultiviewData.ViewCount();
        u32 geometryCount = (u32)cullMultiviewData.Geometries().size();
        multiviewResource.ViewCount = viewCount;
        multiviewResource.GeometryCount = geometryCount;

        multiviewResource.Objects.reserve(geometryCount);
        multiviewResource.Meshlets.reserve(geometryCount);
        multiviewResource.Commands.reserve(geometryCount);

        multiviewResource.HiZs.reserve(viewCount);
        multiviewResource.MeshVisibility.reserve(viewCount);
        multiviewResource.MeshletVisibility.reserve(viewCount);
        /* todo: these two can be optimized:
         * we need additional `geometryCount` buffers only if triangle culling is really used */
        multiviewResource.CompactCommands.reserve(viewCount + geometryCount);

        multiviewResource.ViewSpans = graph.CreateResource(baseName + ".ViewSpans",
            GraphBufferDescription{.SizeBytes = geometryCount * sizeof(CullMultiviewData::ViewSpan)});
        multiviewResource.Views = graph.CreateResource(baseName + ".Views",
            GraphBufferDescription{.SizeBytes = viewCount * sizeof(CullViewDataGPU)});
        multiviewResource.HiZSampler = cullMultiviewData.View(0).Static.HiZContext->GetMinMaxSampler(
            HiZReductionMode::Min);

        multiviewResource.CompactCommandCount = graph.CreateResource(
            std::format("{}.CompactCommandsCount", baseName),
            GraphBufferDescription{.SizeBytes = (viewCount + geometryCount) * sizeof(u32)});
        multiviewResource.CompactCommandCountReocclusion = graph.CreateResource(
            std::format("{}.CompactCommandsCount.Reocclusion", baseName),
            GraphBufferDescription{.SizeBytes = viewCount * sizeof(u32)});

        for (u32 i = 0; i < geometryCount; i++)
        {
            auto* geometry = cullMultiviewData.Geometries()[i];

            multiviewResource.Objects.push_back(graph.AddExternal(std::format("{}.Objects.{}", baseName, i),
                geometry->GetRenderObjectsBuffer()));
            multiviewResource.Meshlets.push_back(graph.AddExternal(std::format("{}.Meshlets.{}", baseName, i),
                geometry->GetMeshletsBuffer()));
            multiviewResource.Commands.push_back(graph.AddExternal(std::format("{}.Commands.{}", baseName, i),
                geometry->GetCommandsBuffer()));
        }

        
        for (u32 i = 0; i < viewCount; i++)
        {
            auto& view = cullMultiviewData.View(i);
            auto&& [staticV, dynamicV] = view;
            
            multiviewResource.HiZs.push_back(graph.AddExternal(std::format("{}.HiZ.{}", baseName, i),
                staticV.HiZContext->GetHiZPrevious(HiZReductionMode::Min)->get(),
                ImageUtils::DefaultTexture::Black));

            multiviewResource.MeshVisibility.push_back(graph.AddExternal(
                std::format("{}.Visibility.Mesh.{}", baseName, i),
                cullMultiviewData.Visibilities()[i].Mesh()));
            multiviewResource.MeshletVisibility.push_back(graph.AddExternal(
                std::format("{}.Visibility.Meshlet.{}", baseName, i),
                cullMultiviewData.Visibilities()[i].Meshlet()));

            multiviewResource.CompactCommands.push_back(graph.CreateResource(
                std::format("{}.CompactCommands.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = staticV.Geometry->GetCommandsBuffer().GetSizeBytes()}));
        }

        /* create `geometryCount` additional command and command count buffers for triangle culling */
        for (u32 i = 0; i < geometryCount; i++)
        {
            auto* geometry = cullMultiviewData.Geometries()[i];
            multiviewResource.CompactCommands.push_back(graph.CreateResource(
                std::format("{}.CompactCommands.Combined.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = geometry->GetCommandsBuffer().GetSizeBytes()}));
        }

        return multiviewResource;
    }

    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform);
        multiview.Views = graph.Read(multiview.Views, Compute | Uniform);
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
            multiview.Objects[i] = graph.Read(multiview.Objects[i], Compute | Storage);
        
        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            multiview.HiZs[i] = graph.Read(multiview.HiZs[i], Compute | Sampled);
            multiview.MeshVisibility[i] = graph.Read(multiview.MeshVisibility[i], Compute | Storage);
            multiview.MeshVisibility[i] = graph.Write(multiview.MeshVisibility[i], Compute | Storage);
        }

        /* init view buffers */
        graph.Upload(multiview.ViewSpans, multiview.Multiview->ViewSpans());
        std::vector<CullViewDataGPU> views = multiview.Multiview->CreateMultiviewGPU();
        graph.Upload(multiview.Views, views);
    }

    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform);
        multiview.Views = graph.Read(multiview.Views, Compute | Uniform);

        if (cullStage != CullStage::Reocclusion)
        {
            multiview.CompactCommandCount = graph.Read(multiview.CompactCommandCount, Compute | Storage);
            multiview.CompactCommandCount = graph.Write(multiview.CompactCommandCount, Compute | Storage);
        }
        else
        {
            multiview.CompactCommandCountReocclusion = graph.Read(
                multiview.CompactCommandCountReocclusion, Compute | Storage);
            multiview.CompactCommandCountReocclusion = graph.Write(
                multiview.CompactCommandCountReocclusion, Compute | Storage);
        }
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            multiview.Objects[i] = graph.Read(multiview.Objects[i], Compute | Storage);
            multiview.Meshlets[i] = graph.Read(multiview.Meshlets[i], Compute | Storage);
            multiview.Commands[i] = graph.Read(multiview.Commands[i], Compute | Storage);
        }
        
        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            multiview.HiZs[i] = graph.Read(multiview.HiZs[i], Compute | Sampled);
            multiview.MeshVisibility[i] = graph.Read(multiview.MeshVisibility[i], Compute | Storage);
            multiview.MeshletVisibility[i] = graph.Read(multiview.MeshletVisibility[i], Compute | Storage);
            multiview.MeshletVisibility[i] = graph.Read(multiview.MeshletVisibility[i], Compute | Storage);

            multiview.CompactCommands[i] = graph.Write(multiview.CompactCommands[i], Compute | Storage);
        }

        /* read-write `geometryCount` additional command and command count buffers for triangle culling
         * we don't need it in `Reocclusion` stage
         */
        if (cullStage != CullStage::Reocclusion)
        {
            for (u32 i = 0; i < multiview.GeometryCount; i++)
            {
                u32 index = multiview.ViewCount + i;
                multiview.CompactCommands[index] = graph.Write(multiview.CompactCommands[index], Compute | Storage);
            }
        }

        /* initialize count with zeros */
        if (cullStage == CullStage::Reocclusion)
            for (u32 i = 0; i < multiview.ViewCount; i++)
                graph.Upload(multiview.CompactCommandCountReocclusion, 0u, i * sizeof(u32));
        else
            for (u32 i = 0; i < multiview.ViewCount + multiview.GeometryCount; i++)
                graph.Upload(multiview.CompactCommandCount, 0u, i * sizeof(u32));
    }

    CullTrianglesMultiviewResource createTriangleCullMultiview(CullMultiviewResources& multiview, Graph& graph,
        const std::string& baseName)
    {
        CullTrianglesMultiviewResource multiviewResource = {};

        multiviewResource.MeshletCull = &multiview;
        multiviewResource.Multiview = multiview.Multiview;
        u32 viewCount = multiview.Multiview->TriangleViewCount();
        if (viewCount == 0)
            return multiviewResource;
        
        multiviewResource.TriangleViewCount = viewCount;
        multiviewResource.MeshletViewIndices.reserve(viewCount);

        multiviewResource.Indices.reserve(multiview.GeometryCount);
        multiviewResource.BatchDispatches.reserve(multiview.GeometryCount);
        multiviewResource.AttributeBuffers.reserve(multiview.GeometryCount);

        multiviewResource.TriangleVisibility.reserve(viewCount);
        multiviewResource.Triangles.reserve(viewCount);
        multiviewResource.IndicesCulled.reserve(viewCount);

        multiviewResource.Cameras.reserve(viewCount);
        multiviewResource.SceneLights.resize(viewCount, {});
        multiviewResource.AttachmentResources.resize(viewCount, {});
        multiviewResource.IBLs.resize(viewCount, {});
        multiviewResource.SSAOs.resize(viewCount, {});
        multiviewResource.CSMs.resize(viewCount, {});
        
        multiviewResource.ViewSpans = graph.CreateResource(baseName + ".ViewSpans",
            GraphBufferDescription{.SizeBytes = multiview.GeometryCount * sizeof(CullMultiviewData::ViewSpan)});
        multiviewResource.Views = graph.CreateResource(baseName + ".Views",
            GraphBufferDescription{.SizeBytes = viewCount * sizeof(CullViewDataGPU)});

        for (u32 i = 0; i < multiview.Multiview->ViewCount(); i++)
            multiviewResource.MaxDispatches = std::max(multiviewResource.MaxDispatches,
                multiview.Multiview->View(i).Static.Geometry->GetCommandCount());
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            auto* geometry = multiview.Multiview->View(i).Static.Geometry;

            multiviewResource.Indices.push_back(graph.AddExternal(std::format("{}.Indices.{}", baseName, i),
                geometry->GetAttributeBuffers().Indices));

            multiviewResource.BatchDispatches.push_back(graph.CreateResource(
               std::format("{}.Dispatches.{}", baseName, i),
               GraphBufferDescription{.SizeBytes = multiviewResource.MaxDispatches * sizeof(IndirectDispatchCommand)}));

            /* draw attribute buffers */
            multiviewResource.AttributeBuffers.push_back(createDrawAttributes(*geometry, graph, baseName));
        }

        u32 triangleViewIndex = 0;
        for (u32 meshletViewIndex = 0; meshletViewIndex < multiview.ViewCount; meshletViewIndex++)
        {
            auto& view = multiview.Multiview->View(meshletViewIndex);
            auto&& [staticV, dynamicV] = view;

            /* skip all views that do not involve triangle culling */
            if (!staticV.CullTriangles)
                continue;

            multiviewResource.MeshletViewIndices.push_back(meshletViewIndex);

            multiviewResource.TriangleVisibility.push_back(graph.AddExternal(
                std::format("{}.Visibility.Triangle.{}", baseName, triangleViewIndex),
                multiview.Multiview->TriangleVisibilities()[triangleViewIndex].Triangle()));

            multiviewResource.Triangles.push_back({});
            for (auto& triangles : multiviewResource.Triangles.back())
                triangles = graph.CreateResource(std::format("{}.Triangles.{}", baseName, triangleViewIndex),
                        GraphBufferDescription{
                            .SizeBytes = TriangleCullMultiviewTraits::TriangleCount() *
                                sizeof(TriangleCullMultiviewTraits::TriangleType)});

            multiviewResource.IndicesCulled.push_back({});
            for (auto& indices : multiviewResource.IndicesCulled.back())
                indices = graph.CreateResource(std::format("{}.Indices.Culled.{}", baseName, triangleViewIndex),
                    GraphBufferDescription{
                        .SizeBytes = TriangleCullMultiviewTraits::IndexCount() *
                            sizeof(TriangleCullMultiviewTraits::IndexType)});

            multiviewResource.Cameras.push_back(graph.CreateResource(
                std::format("{}.Draw.Camera.{}", baseName, triangleViewIndex),
                GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)}));

            triangleViewIndex++;
        }

        for (u32 i = 0; i < TriangleCullMultiviewTraits::MAX_BATCHES; i++)
        {
            multiviewResource.Draws[i] = graph.CreateResource(std::format("{}.Draws", baseName),
                GraphBufferDescription{.SizeBytes = viewCount * sizeof(IndirectDrawCommand)});
            
            multiviewResource.IndicesCulledCount[i] = graph.CreateResource(
                std::format("{}.CulledCount", baseName),
                GraphBufferDescription{.SizeBytes = viewCount * sizeof(u32)});
        }
        
        return multiviewResource;
    }

    void readWriteCullTrianglePrepareMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.MeshletCull->CompactCommandCount = graph.Read(multiview.MeshletCull->CompactCommandCount,
            Compute | Storage);
        
        for (u32 i = 0; i < multiview.MeshletCull->GeometryCount; i++)
            multiview.BatchDispatches[i] = graph.Write(multiview.BatchDispatches[i], Compute | Storage);
    }

    void readWriteCullTriangleMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        ASSERT(multiview.AttachmentsRenames != nullptr, "Attachment renames are not provided")

        u32 geometryCount = multiview.MeshletCull->GeometryCount;
        u32 viewCount = multiview.TriangleViewCount;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform);
        multiview.Views = graph.Read(multiview.Views, Compute | Uniform);

        multiview.MeshletCull->CompactCommandCount = graph.Read(
            multiview.MeshletCull->CompactCommandCount, Compute | Storage);
        
        for (u32 i = 0; i < geometryCount; i++)
        {
            multiview.Indices[i] = graph.Read(multiview.Indices[i], Compute | Storage);
            multiview.BatchDispatches[i] = graph.Read(multiview.BatchDispatches[i], Indirect);

            multiview.MeshletCull->Objects[i] = graph.Read(multiview.MeshletCull->Objects[i], Compute | Storage);
        }

        for (u32 i = 0; i < viewCount; i++)
        {
            u32 meshletIndex = multiview.MeshletViewIndices[i];

            multiview.MeshletCull->HiZs[meshletIndex] = graph.Read(
                multiview.MeshletCull->HiZs[meshletIndex], Compute | Sampled);

            multiview.MeshletCull->MeshletVisibility[meshletIndex] = graph.Read(
                multiview.MeshletCull->MeshletVisibility[meshletIndex], Compute | Storage);
            multiview.MeshletCull->MeshletVisibility[meshletIndex] = graph.Write(
                multiview.MeshletCull->MeshletVisibility[meshletIndex], Compute | Storage);

            multiview.MeshletCull->CompactCommands[meshletIndex] = graph.Read(
                multiview.MeshletCull->CompactCommands[meshletIndex], Compute | Storage);
            
            multiview.TriangleVisibility[i] = graph.Read(
                multiview.TriangleVisibility[i], Compute | Storage);
            multiview.TriangleVisibility[i] = graph.Write(
                multiview.TriangleVisibility[i], Compute | Storage);

            for (u32 batch = 0; batch < TriangleCullMultiviewTraits::MAX_BATCHES; batch++)
            {
                multiview.Triangles[i][batch] = graph.Write(multiview.Triangles[i][batch], Compute | Storage);
                
                multiview.IndicesCulled[i][batch] = graph.Write(multiview.IndicesCulled[i][batch],
                    Compute | Storage | Index);
            }
        }

        for (u32 batch = 0; batch < TriangleCullMultiviewTraits::MAX_BATCHES; batch++)
        {
            multiview.IndicesCulledCount[batch] = graph.Write(multiview.IndicesCulledCount[batch], Compute | Storage);
            multiview.IndicesCulledCount[batch] = graph.Read(multiview.IndicesCulledCount[batch], Compute | Storage);
        }

        /* read-write `geometryCount` additional command and command count buffers for triangle culling */
        for (u32 i = 0; i < geometryCount; i++)
        {
            u32 index = multiview.MeshletCull->ViewCount + i;

            multiview.MeshletCull->CompactCommands[index] = graph.Read(
                multiview.MeshletCull->CompactCommands[index], Compute | Storage);
        }

        /* here begins all the draw-related reads and writes
         * it is separated from culling stuff simply for better readability
         */
        for (u32 i = 0; i < viewCount; i++)
        {
            auto& view = multiview.MeshletCull->Multiview->TriangleView(i);
            auto&& [staticV, dynamicV] = view;
            
            multiview.Cameras[i] = graph.Read(multiview.Cameras[i], Vertex | Pixel | Uniform);
            
            /* read and update attachment handles */
            Utils::updateRecordedAttachmentResources(dynamicV.DrawInfo.Attachments, *multiview.AttachmentsRenames);
            // todo: this is bad, change it after addition of 'mutableResources'
            auto attachments = dynamicV.DrawInfo.Attachments;
            multiview.AttachmentResources[i] = readWriteDrawAttachments(dynamicV.DrawInfo.Attachments, graph);
            Utils::recordUpdatedAttachmentResources(attachments, multiview.AttachmentResources[i],
                *multiview.AttachmentsRenames);

            for (u32 batch = 0; batch < TriangleCullMultiviewTraits::MAX_BATCHES; batch++)
            {
                multiview.Draws[batch] = graph.Write(multiview.Draws[batch], Compute | Storage);
                multiview.Draws[batch] = graph.Read(multiview.Draws[batch], Indirect);
            }
        }

        for (u32 i = 0; i < geometryCount; i++)
        {
            readDrawAttributes(multiview.AttributeBuffers[i], graph, Vertex);
            multiview.MeshletCull->Commands[i] = graph.Read(multiview.MeshletCull->Commands[i],
                Vertex | Pixel | Storage);
        }

        /* upload view data */
        graph.Upload(multiview.ViewSpans, multiview.Multiview->TriangleViewSpans());
        std::vector<CullViewDataGPU> views = multiview.Multiview->CreateMultiviewGPUTriangles();
        graph.Upload(multiview.Views, views);
        for (u32 i = 0; i < multiview.TriangleViewCount; i++)
        {
            auto& view = multiview.Multiview->TriangleView(i);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*view.Dynamic.Camera, view.Dynamic.Resolution);
            graph.Upload(multiview.Cameras[i], cameraGPU);

            for (u32 batchIndex = 0; batchIndex < TriangleCullMultiviewTraits::MAX_BATCHES; batchIndex++)
                graph.Upload(multiview.IndicesCulledCount[batchIndex], 0, i * sizeof(u32));
        }
    }
}
