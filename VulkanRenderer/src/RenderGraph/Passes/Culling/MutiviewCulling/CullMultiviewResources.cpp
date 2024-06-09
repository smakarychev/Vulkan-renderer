#include "CullMultiviewResources.h"

#include "CullMultiviewData.h"
#include "Scene/SceneGeometry.h"

namespace RG::RgUtils
{
    CullMultiviewResources createCullMultiview(CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName)
    {
        CullMultiviewResources multiviewResource = {};
        
        multiviewResource.Multiview = &cullMultiviewData;
        u32 viewCount = (u32)cullMultiviewData.Views().size();
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
        multiviewResource.CompactCommandCount.reserve(viewCount + geometryCount);
        
        multiviewResource.CompactCommandCountReocclusion.reserve(viewCount);

        multiviewResource.HiZSampler = cullMultiviewData.Views()[0].Static.HiZContext->GetSampler();

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
        
        multiviewResource.ViewSpans = graph.CreateResource(baseName + ".ViewSpans",
            GraphBufferDescription{.SizeBytes = geometryCount * sizeof(CullMultiviewData::ViewSpan)});
        multiviewResource.Views = graph.CreateResource(baseName + ".Views",
            GraphBufferDescription{
                .SizeBytes = viewCount * sizeof(CullViewDataGPU)});
        
        for (u32 i = 0; i < viewCount; i++)
        {
            auto& view = cullMultiviewData.Views()[i];
            auto&& [staticV, dynamicV] = view;
            
            multiviewResource.HiZs.push_back(graph.AddExternal(std::format("{}.HiZ.{}", baseName, i),
                staticV.HiZContext->GetHiZPrevious()->get(),
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
            multiviewResource.CompactCommandCount.push_back(graph.CreateResource(
                std::format("{}.CompactCommandsCount.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = sizeof(u32)}));
            multiviewResource.CompactCommandCountReocclusion.push_back(graph.CreateResource(
                std::format("{}.CompactCommandsCount.Reocclusion.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = sizeof(u32)}));
        }

        /* create `geometryCount` additional command and command count buffers for triangle culling */
        for (u32 i = 0; i < geometryCount; i++)
        {
            auto* geometry = cullMultiviewData.Geometries()[i];
            multiviewResource.CompactCommands.push_back(graph.CreateResource(
                std::format("{}.CompactCommands.Combined.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = geometry->GetCommandsBuffer().GetSizeBytes()}));
            multiviewResource.CompactCommandCount.push_back(graph.CreateResource(
                std::format("{}.CompactCommandsCount.Combined.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = sizeof(u32)}));
        }

        return multiviewResource;
    }

    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform | Upload);
        multiview.Views = graph.Read(multiview.Views, Compute | Uniform | Upload);
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
            multiview.Objects[i] = graph.Read(multiview.Objects[i], Compute | Storage);
        
        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            multiview.HiZs[i] = graph.Read(multiview.HiZs[i], Compute | Sampled);
            multiview.MeshVisibility[i] = graph.Read(multiview.MeshVisibility[i], Compute | Storage);
            multiview.MeshVisibility[i] = graph.Write(multiview.MeshVisibility[i], Compute | Storage);
        }
    }

    void updateMeshCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview)
    {
        descriptors.UpdateBinding("u_view_spans", resources.GetBuffer(multiview.ViewSpans).BindingInfo());
        descriptors.UpdateBinding("u_views", resources.GetBuffer(multiview.Views).BindingInfo());

        for (u32 i = 0; i < multiview.GeometryCount; i++)
            descriptors.UpdateBinding("u_objects", resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);
            
            descriptors.UpdateBinding("u_hiz", hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);
            
            descriptors.UpdateBinding("u_object_visibility", resources.GetBuffer(multiview.MeshVisibility[i])
                .BindingInfo(), i);
        }
    }

    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform);
        multiview.Views = graph.Read(multiview.Views, Compute | Uniform);
        
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

            if (cullStage != CullStage::Reocclusion)
            {
                multiview.CompactCommandCount[i] = graph.Read(multiview.CompactCommandCount[i],
                    Compute | Storage | Upload);
                multiview.CompactCommandCount[i] = graph.Write(multiview.CompactCommandCount[i], Compute | Storage);
            }
            else
            {
                multiview.CompactCommandCountReocclusion[i] = graph.Read(
                    multiview.CompactCommandCountReocclusion[i], Compute | Storage | Upload);
                multiview.CompactCommandCountReocclusion[i] = graph.Write(
                    multiview.CompactCommandCountReocclusion[i], Compute | Storage);
            }
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
                multiview.CompactCommandCount[index] = graph.Read(multiview.CompactCommandCount[index],
                    Compute | Storage | Upload);
                multiview.CompactCommandCount[index] = graph.Write(multiview.CompactCommandCount[index],
                    Compute | Storage);
            }
        }
    }

    void updateCullMeshletMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage, ResourceUploader& resourceUploader)
    {
        descriptors.UpdateBinding("u_view_spans", resources.GetBuffer(multiview.ViewSpans).BindingInfo());
        descriptors.UpdateBinding("u_views", resources.GetBuffer(multiview.Views).BindingInfo());
        
        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            descriptors.UpdateBinding("u_objects", resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);
            descriptors.UpdateBinding("u_meshlets", resources.GetBuffer(multiview.Meshlets[i]).BindingInfo(), i);
            descriptors.UpdateBinding("u_commands", resources.GetBuffer(multiview.Commands[i]).BindingInfo(), i);
        }

        for (u32 i = 0; i < multiview.ViewCount; i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);
            
            descriptors.UpdateBinding("u_hiz", hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);
            
            descriptors.UpdateBinding("u_object_visibility", resources.GetBuffer(multiview.MeshVisibility[i])
                .BindingInfo(), i);
            descriptors.UpdateBinding("u_meshlet_visibility", resources.GetBuffer(multiview.MeshletVisibility[i])
                .BindingInfo(), i);
            descriptors.UpdateBinding("u_compacted_commands", resources.GetBuffer(multiview.CompactCommands[i])
                .BindingInfo(), i);

            const Buffer& countBuffer = resources.GetBuffer(cullStage == CullStage::Reocclusion ?
                multiview.CompactCommandCountReocclusion[i] : multiview.CompactCommandCount[i], 0u,
                resourceUploader);
            descriptors.UpdateBinding("u_count", countBuffer.BindingInfo(), i);
        }

        /* update `geometryCount` additional command and command count buffer bindings for triangle culling
         * we don't need it in `Reocclusion` stage
         */
        if (cullStage != CullStage::Reocclusion)
        {
            for (u32 i = 0; i < multiview.GeometryCount; i++)
            {
                u32 index = multiview.ViewCount + i;
                descriptors.UpdateBinding("u_compacted_commands", resources.GetBuffer(multiview.CompactCommands[index])
                    .BindingInfo(), index);

                const Buffer& countBuffer = resources.GetBuffer(multiview.CompactCommandCount[i], 0u, resourceUploader);
                descriptors.UpdateBinding("u_count", countBuffer.BindingInfo(), index);
            }
        }
    }

    CullTrianglesMultiviewResource createTriangleCullMultiview(CullMultiviewResources& multiview, Graph& graph,
        const std::string& baseName)
    {
        CullTrianglesMultiviewResource multiviewResource = {};

        multiviewResource.CullResources = &multiview;
        u32 viewCount = (u32)multiview.Multiview->TriangleVisibilities().size();
        multiviewResource.TriangleViewCount = viewCount;

        multiviewResource.Indices.reserve(multiview.GeometryCount);
        
        multiviewResource.BatchDispatches.reserve(multiview.GeometryCount);
        multiviewResource.TriangleVisibility.reserve(multiview.GeometryCount);

        multiviewResource.Triangles.reserve(viewCount);
        multiviewResource.IndicesCulled.reserve(viewCount);
        multiviewResource.IndicesCulledCount.reserve(viewCount);
        multiviewResource.Draw.reserve(viewCount);

        for (u32 i = 0; i < multiview.GeometryCount; i++)
        {
            auto* geometry = multiview.Multiview->Views()[i].Static.Geometry;

            multiviewResource.Indices.push_back(graph.AddExternal(std::format("{}.Indices.{}", baseName, i),
                geometry->GetAttributeBuffers().Indices));

            u32 maxDispatches = TriangleCullMultiviewTraits::MaxDispatches(geometry->GetCommandCount());

            multiviewResource.BatchDispatches.push_back(graph.CreateResource(
               std::format("{}.Dispatches.{}", baseName, i),
               GraphBufferDescription{.SizeBytes = maxDispatches * sizeof(IndirectDispatchCommand)}));
        }

        multiviewResource.MaxDispatches = graph.CreateResource(baseName + ".MaxDispatches",
            GraphBufferDescription{.SizeBytes = multiview.GeometryCount * sizeof(u32)});

        u32 triangleViewIndex = 0;
        for (u32 meshletViewIndex = 0; meshletViewIndex < multiview.ViewCount; meshletViewIndex++)
        {
            auto& view = multiview.Multiview->Views()[meshletViewIndex];
            auto&& [staticV, dynamicV] = view;

            // skip all views that do not involve triangle culling
            if (!staticV.CullTriangles)
                continue;

            multiviewResource.MeshletViewIndices.push_back(meshletViewIndex);
            
            multiviewResource.TriangleVisibility.push_back(graph.AddExternal(
                std::format("{}.Visibility.Triangle.{}", baseName, triangleViewIndex),
                multiview.Multiview->TriangleVisibilities()[triangleViewIndex].Triangle()));

            multiviewResource.Triangles.push_back({});
            for (auto& triangles : multiviewResource.Triangles.back())
                triangles = graph.CreateResource(
                        std::format("{}.Triangles.{}", baseName, triangleViewIndex),
                        GraphBufferDescription{
                            .SizeBytes = TriangleCullMultiviewTraits::TriangleCount() *
                                sizeof(TriangleCullMultiviewTraits::TriangleType)});

            multiviewResource.IndicesCulled.push_back({});
            for (auto& indices : multiviewResource.IndicesCulled.back())
                indices = graph.CreateResource(
                    std::format("{}.Indices.Culled.{}", baseName, triangleViewIndex),
                    GraphBufferDescription{
                        .SizeBytes = TriangleCullMultiviewTraits::IndexCount() *
                            sizeof(TriangleCullMultiviewTraits::IndexType)});

            multiviewResource.IndicesCulledCount.push_back({});
            for (auto& count : multiviewResource.IndicesCulledCount.back())
                count = graph.CreateResource(
                    std::format("{}.CulledCount.{}", baseName, triangleViewIndex),
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});

            multiviewResource.Draw.push_back({});
            for (auto& draw : multiviewResource.Draw.back())
                draw = graph.CreateResource(
                    std::format("{}.Draw.{}", baseName, triangleViewIndex),
                    GraphBufferDescription{.SizeBytes = sizeof(IndirectDrawCommand)});
                    
            triangleViewIndex++;
        }

        return multiviewResource;
    }

    void readWriteCullTrianglePrepareMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.MaxDispatches = graph.Read(multiview.MaxDispatches, Compute | Uniform | Upload);

        for (u32 i = 0; i < multiview.CullResources->GeometryCount; i++)
        {
            u32 compactCountIndex = i + multiview.CullResources->ViewCount;
            multiview.BatchDispatches[i] = graph.Write(multiview.BatchDispatches[i], Compute | Storage);
            multiview.CullResources->CompactCommandCount[compactCountIndex] = graph.Read(
                multiview.CullResources->CompactCommandCount[compactCountIndex],
                Compute | Storage);
        }
    }

    void updateCullTrianglePrepareMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullTrianglesMultiviewResource& multiview)
    {
        descriptors.UpdateBinding("u_max_dispatches", resources.GetBuffer(multiview.MaxDispatches).BindingInfo());

        for (u32 i = 0; i < multiview.CullResources->GeometryCount; i++)
        {
            u32 compactCountIndex = i + multiview.CullResources->ViewCount;
            descriptors.UpdateBinding("u_command_counts", resources.GetBuffer(
                multiview.CullResources->CompactCommandCount[compactCountIndex])
                .BindingInfo(), i);
            descriptors.UpdateBinding("u_dispatches", resources.GetBuffer(multiview.BatchDispatches[i])
                .BindingInfo(), i);
        }
    }
}
