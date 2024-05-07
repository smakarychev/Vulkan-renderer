#include "CullMultiviewResource.h"

#include "CullMultiviewData.h"
#include "Scene/SceneGeometry.h"

namespace RG::RgUtils
{
    CullMultiviewResource createCullMultiview(const CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName)
    {
        CullMultiviewResource multiviewResource = {};

        multiviewResource.Objects.reserve(cullMultiviewData.Geometries().size());
        multiviewResource.Meshlets.reserve(cullMultiviewData.Geometries().size());
        multiviewResource.Commands.reserve(cullMultiviewData.Geometries().size());

        
        multiviewResource.ViewDescriptions = &cullMultiviewData.Views();
        multiviewResource.HiZs.reserve(cullMultiviewData.Views().size());
        multiviewResource.Views.reserve(cullMultiviewData.Views().size());
        multiviewResource.MeshVisibility.reserve(cullMultiviewData.Views().size());
        multiviewResource.MeshletVisibility.reserve(cullMultiviewData.Views().size());
        multiviewResource.CompactCommands.reserve(cullMultiviewData.Views().size());
        multiviewResource.CompactCommandCount.reserve(cullMultiviewData.Views().size());
        multiviewResource.CompactCommandCountReocclusion.reserve(cullMultiviewData.Views().size());
        multiviewResource.CommandFlags.reserve(cullMultiviewData.Views().size());

        multiviewResource.HiZSampler = cullMultiviewData.Views()[0].Static.HiZContext->GetSampler();

        for (u32 i = 0; i < cullMultiviewData.Geometries().size(); i++)
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
            GraphBufferDescription{
                .SizeBytes = cullMultiviewData.Geometries().size() * sizeof(CullMultiviewData::ViewSpan)});
        
        for (u32 i = 0; i < cullMultiviewData.Views().size(); i++)
        {
            auto& view = cullMultiviewData.Views()[i];
            auto&& [staticV, dynamicV] = view;
            
            multiviewResource.HiZs.push_back(graph.AddExternal(std::format("{}.HiZ.{}", baseName, i),
                staticV.HiZContext->GetHiZPrevious()->get(),
                ImageUtils::DefaultTexture::Black));

            multiviewResource.Views.push_back(graph.CreateResource(std::format("{}.View.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = sizeof(CullViewDataGPU)}));

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
            
            multiviewResource.CommandFlags.push_back(graph.CreateResource(
                  std::format("{}.CompactCommandsCount.CommandFlags.{}", baseName, i),
                GraphBufferDescription{.SizeBytes = staticV.Geometry->GetMeshletCount() * sizeof(u8)}));
        }

        return multiviewResource;
    }

    void readWriteCullMeshMultiview(CullMultiviewResource& multiview, Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform | Upload);
        
        for (u32 i = 0; i < multiview.Objects.size(); i++)
            multiview.Objects[i] = graph.Read(multiview.Objects[i], Compute | Storage);
        
        for (u32 i = 0; i < multiview.Views.size(); i++)
        {
            multiview.HiZs[i] = graph.Read(multiview.HiZs[i], Compute | Sampled);
            multiview.Views[i] = graph.Read(multiview.Views[i], Compute | Uniform | Upload);
            multiview.MeshVisibility[i] = graph.Read(multiview.MeshVisibility[i], Compute | Storage);
            multiview.MeshVisibility[i] = graph.Write(multiview.MeshVisibility[i], Compute | Storage);
        }
    }

    void updateMeshCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResource& multiview)
    {
        descriptors.UpdateBinding("u_view_spans", resources.GetBuffer(multiview.ViewSpans).BindingInfo());

        for (u32 i = 0; i < multiview.Objects.size(); i++)
            descriptors.UpdateBinding("u_objects", resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);

        for (u32 i = 0; i < multiview.Views.size(); i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);
            
            descriptors.UpdateBinding("u_hiz", hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);
            
            descriptors.UpdateBinding("u_views", resources.GetBuffer(multiview.Views[i]).BindingInfo(), i);
            descriptors.UpdateBinding("u_object_visibility", resources.GetBuffer(multiview.MeshVisibility[i])
                .BindingInfo(), i);
        }
    }

    void readWriteCullMeshletMultiview(CullMultiviewResource& multiview, CullStage cullStage, bool triangleCull,
        Graph& graph)
    {
        using enum ResourceAccessFlags;

        multiview.ViewSpans = graph.Read(multiview.ViewSpans, Compute | Uniform | Upload);
        
        for (u32 i = 0; i < multiview.Objects.size(); i++)
        {
            multiview.Objects[i] = graph.Read(multiview.Objects[i], Compute | Storage);
            multiview.Meshlets[i] = graph.Read(multiview.Meshlets[i], Compute | Storage);
            multiview.Commands[i] = graph.Read(multiview.Commands[i], Compute | Storage);
        }
        
        for (u32 i = 0; i < multiview.Views.size(); i++)
        {
            multiview.HiZs[i] = graph.Read(multiview.HiZs[i], Compute | Sampled);
            multiview.Views[i] = graph.Read(multiview.Views[i], Compute | Uniform | Upload);
            multiview.MeshVisibility[i] = graph.Read(multiview.MeshVisibility[i], Compute | Storage);
            multiview.MeshletVisibility[i] = graph.Read(multiview.MeshletVisibility[i], Compute | Storage);
            multiview.MeshletVisibility[i] = graph.Read(multiview.MeshletVisibility[i], Compute | Storage);

            multiview.CompactCommands[i] = graph.Write(multiview.CompactCommands[i], Compute | Storage);

            if (cullStage != CullStage::Reocclusion)
            {
                multiview.CompactCommandCount[i] = graph.Read(multiview.CompactCommandCount[i], Compute | Storage);
                multiview.CompactCommandCount[i] = graph.Write(multiview.CompactCommandCount[i], Compute | Storage);
            }
            else
            {
                multiview.CompactCommandCountReocclusion[i] = graph.Read(
                    multiview.CompactCommandCountReocclusion[i], Compute | Storage);
                multiview.CompactCommandCountReocclusion[i] = graph.Write(
                    multiview.CompactCommandCountReocclusion[i], Compute | Storage);

                if (triangleCull)
                    multiview.CommandFlags[i] = graph.Read(multiview.CommandFlags[i], Compute | Storage);
            }
            if (triangleCull)
                multiview.CommandFlags[i] = graph.Write(multiview.CommandFlags[i], Compute | Storage);
        }
    }

    void updateMeshletCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResource& multiview, CullStage cullStage, bool triangleCull,
        ResourceUploader& resourceUploader)
    {
        descriptors.UpdateBinding("u_view_spans", resources.GetBuffer(multiview.ViewSpans).BindingInfo());

        for (u32 i = 0; i < multiview.Objects.size(); i++)
        {
            descriptors.UpdateBinding("u_objects", resources.GetBuffer(multiview.Objects[i]).BindingInfo(), i);
            descriptors.UpdateBinding("u_meshlets", resources.GetBuffer(multiview.Meshlets[i]).BindingInfo(), i);
            descriptors.UpdateBinding("u_commands", resources.GetBuffer(multiview.Commands[i]).BindingInfo(), i);
        }

        for (u32 i = 0; i < multiview.Views.size(); i++)
        {
            const Texture& hiz = resources.GetTexture(multiview.HiZs[i]);
            
            descriptors.UpdateBinding("u_hiz", hiz.BindingInfo(multiview.HiZSampler,
                hiz.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly), i);
            
            descriptors.UpdateBinding("u_views", resources.GetBuffer(multiview.Views[i]).BindingInfo(), i);
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

            if (triangleCull)
                descriptors.UpdateBinding("u_flags", resources.GetBuffer(multiview.CommandFlags[i]).BindingInfo(), i);
        }
    }
}
