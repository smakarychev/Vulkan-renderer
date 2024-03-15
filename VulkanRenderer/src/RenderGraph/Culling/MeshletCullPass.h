#pragma once

#include "MeshCullPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "Rendering/Buffer.h"

class MeshCullContext;

class MeshletCullContext
{
public:
    MeshletCullContext(const MeshCullContext& meshCullContext);

    const Buffer& Visibility() const { return m_Visibility; }
    const RenderPassGeometry& Geometry() const { return *m_Geometry; }
private:
    Buffer m_Visibility;
    
    const RenderPassGeometry* m_Geometry{nullptr};
};

template <bool Reocclusion>
class MeshletCullPassGeneral
{
public:
    struct PassData
    {
        RenderGraph::Resource HiZ;
        Sampler HiZSampler;
        RenderGraph::Resource SceneUbo;
        RenderGraph::Resource ObjectsSsbo;
        RenderGraph::Resource ObjectVisibilitySsbo;
        RenderGraph::Resource MeshletsSsbo;
        RenderGraph::Resource MeshletVisibilitySsbo;
        RenderGraph::Resource CommandsSsbo;
        RenderGraph::Resource CompactCommandsSsbo;
        RenderGraph::Resource CompactCountSsbo;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    MeshletCullPassGeneral(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, const MeshletCullContext& ctx,
        const HiZPassContext& hiZPassContext);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData;
};


using MeshletCullPass = MeshletCullPassGeneral<false>;
using MeshletCullReocclusionPass = MeshletCullPassGeneral<true>;


template <bool Reocclusion>
MeshletCullPassGeneral<Reocclusion>::MeshletCullPassGeneral(RenderGraph::Graph& renderGraph)
{
    ShaderPipelineTemplate* mesheletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/meshlet-cull-comp.shader"},
        "render-graph-meshlet-cull-pass-template", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(mesheletCullTemplate)
        .AddSpecialization("REOCCLUSION", Reocclusion)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(mesheletCullTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(mesheletCullTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

template <bool Reocclusion>
void MeshletCullPassGeneral<Reocclusion>::AddToGraph(RenderGraph::Graph& renderGraph, const MeshletCullContext& ctx,
    const HiZPassContext& hiZPassContext)
{
    using namespace RenderGraph;

    static ShaderDescriptors::BindingInfo samplerBinding =
        m_PipelineData.SamplerDescriptors.GetBindingInfo("u_sampler");
    static ShaderDescriptors::BindingInfo hizBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_hiz");
    static ShaderDescriptors::BindingInfo sceneBinding = 
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_scene_data");
    static ShaderDescriptors::BindingInfo objectsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_objects");
    static ShaderDescriptors::BindingInfo objectVisibilityBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_object_visibility");
    static ShaderDescriptors::BindingInfo meshletsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_meshlets");
    static ShaderDescriptors::BindingInfo meshletVisibilityBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_meshlet_visibility");
    static ShaderDescriptors::BindingInfo commandsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_commands");
    static ShaderDescriptors::BindingInfo compactCommandsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_compacted_commands");
    static ShaderDescriptors::BindingInfo countBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_count");

    static constexpr std::string_view PASS_NAME = Reocclusion ? "meshlet-cull-reocclusion" : "meshlet-cull";

    m_Pass = &renderGraph.AddRenderPass<PassData>(std::string{PASS_NAME},
        [&](Graph& graph, PassData& passData)
        {
            // if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass
            if constexpr(!Reocclusion)
            {
                // we do not really need hiz for ordinary pass, but it is still has to be provided
                auto& meshOutput = graph.GetBlackboard().GetOutput<MeshCullPass::PassData>();
                passData.HiZ = meshOutput.HiZ;
                passData.SceneUbo = meshOutput.SceneUbo;
                passData.ObjectsSsbo = meshOutput.ObjectsSsbo;
                passData.ObjectVisibilitySsbo = meshOutput.VisibilitySsbo;
                passData.MeshletsSsbo = graph.AddExternal("meshlet-cull-meshlets",
                    ctx.Geometry().GetMeshletsBuffer());
                passData.MeshletVisibilitySsbo = graph.AddExternal("meshlet-cull-meshlet-visibility", ctx.Visibility());
                passData.CommandsSsbo = graph.AddExternal("meshlet-cull-commands", ctx.Geometry().GetCommandsBuffer());
                passData.CompactCommandsSsbo = graph.CreateResource("meshlet-cull-compacted-commands",
                    GraphBufferDescription{.SizeBytes = ctx.Geometry().GetCommandsBuffer().GetSizeBytes()});
                // count buffer will be separate for ordinary and reocclusion passes
                passData.CompactCountSsbo = graph.CreateResource("meshlet-cull-command-count",
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});
            }
            else
            {
                passData.HiZ = graph.GetBlackboard().GetOutput<HiZPass::PassData>().HiZOut;
                
                auto& meshletOutput = graph.GetBlackboard().GetOutput<MeshletCullPass::PassData>();
                passData.SceneUbo = meshletOutput.SceneUbo;
                passData.ObjectsSsbo = meshletOutput.ObjectsSsbo;
                passData.ObjectVisibilitySsbo = meshletOutput.ObjectVisibilitySsbo;
                passData.MeshletsSsbo = meshletOutput.MeshletsSsbo;
                passData.MeshletVisibilitySsbo = meshletOutput.MeshletVisibilitySsbo;
                passData.CommandsSsbo = meshletOutput.CommandsSsbo;
                passData.CompactCommandsSsbo = meshletOutput.CompactCommandsSsbo;
                // count buffer will be separate for ordinary and reocclusion passes
                passData.CompactCountSsbo = graph.CreateResource("meshlet-cull-reocclusion-command-count",
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});
            }

            const ResourceAccessFlags commonFlags = ResourceAccessFlags::Compute;

            passData.HiZSampler = hiZPassContext.GetSampler();
            passData.HiZ = graph.Read(passData.HiZ, commonFlags | ResourceAccessFlags::Sampled);
            passData.SceneUbo = graph.Read(passData.SceneUbo, commonFlags | ResourceAccessFlags::Uniform);
            passData.ObjectsSsbo = graph.Read(passData.ObjectsSsbo, commonFlags | ResourceAccessFlags::Storage);
            passData.ObjectVisibilitySsbo = graph.Read(passData.ObjectVisibilitySsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.MeshletsSsbo = graph.Read(passData.MeshletsSsbo, commonFlags | ResourceAccessFlags::Storage);
            passData.MeshletVisibilitySsbo = graph.Read(passData.MeshletVisibilitySsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.MeshletVisibilitySsbo = graph.Write(passData.MeshletVisibilitySsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.CommandsSsbo = graph.Read(passData.CommandsSsbo, commonFlags | ResourceAccessFlags::Storage);
            passData.CompactCommandsSsbo = graph.Read(passData.CompactCommandsSsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.CompactCommandsSsbo = graph.Write(passData.CompactCommandsSsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.CompactCountSsbo = graph.Read(passData.CompactCountSsbo,
                commonFlags | ResourceAccessFlags::Storage);
            passData.CompactCountSsbo = graph.Write(passData.CompactCountSsbo,
                commonFlags | ResourceAccessFlags::Storage);
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Meshlet Cull")
            
            const Texture& hiz = resources.GetTexture(passData.HiZ);
            const Sampler& hizSampler = passData.HiZSampler;
            const Buffer& sceneUbo = resources.GetBuffer(passData.SceneUbo);
            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& objectVisibilitySsbo = resources.GetBuffer(passData.ObjectVisibilitySsbo);
            const Buffer& meshletsSsbo = resources.GetBuffer(passData.MeshletsSsbo);
            const Buffer& meshletVisibilitySsbo = resources.GetBuffer(passData.MeshletVisibilitySsbo);
            const Buffer& commandsSsbo = resources.GetBuffer(passData.CommandsSsbo);
            const Buffer& compactCommandsSsbo = resources.GetBuffer(passData.CompactCommandsSsbo);
            
            const Buffer& countSsbo = resources.GetBuffer(passData.CompactCountSsbo, 0u,
                *frameContext.ResourceUploader);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding(samplerBinding, hiz.CreateBindingInfo(hizSampler, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(hizBinding, hiz.CreateBindingInfo(hizSampler, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(sceneBinding, sceneUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(objectsBinding, objectsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(objectVisibilityBinding, objectVisibilitySsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(meshletsBinding, meshletsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(meshletVisibilityBinding, meshletVisibilitySsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(commandsBinding, commandsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(compactCommandsBinding, compactCommandsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(countBinding, countSsbo.CreateBindingInfo());

            u32 meshletCount = ctx.Geometry().GetMeshletCount();

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}



