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
    struct PassResources
    {
        RenderGraph::Resource MeshletsSsbo{};
        RenderGraph::Resource VisibilitySsbo{};
        RenderGraph::Resource CommandsSsbo{};
        RenderGraph::Resource CompactCommandsSsbo{};
        RenderGraph::Resource CompactCountSsbo{};
        RenderGraph::Resource CompactCountReocclusionSsbo{};
    };
public:
    MeshletCullContext(MeshCullContext& meshCullContext);

    const Buffer& Visibility() { return m_Visibility; }
    const RenderPassGeometry& Geometry() { return m_MeshCullContext->Geometry(); }
    MeshCullContext& MeshContext() { return *m_MeshCullContext; }
    PassResources& Resources() { return m_Resources; }
private:
    Buffer m_Visibility;

    MeshCullContext* m_MeshCullContext{nullptr};
    PassResources m_Resources{};
};

template <bool Reocclusion>
class MeshletCullPassGeneral
{
public:
    struct PassData
    {
        MeshCullContext::PassResources MeshResources;
        MeshletCullContext::PassResources MeshletResources;

        u32 MeshletCount;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    MeshletCullPassGeneral(RenderGraph::Graph& renderGraph, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, MeshletCullContext& ctx);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::PipelineData m_PipelineData;
};


using MeshletCullPass = MeshletCullPassGeneral<false>;
using MeshletCullReocclusionPass = MeshletCullPassGeneral<true>;


template <bool Reocclusion>
MeshletCullPassGeneral<Reocclusion>::MeshletCullPassGeneral(RenderGraph::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* meshletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/meshlet-cull-comp.shader"},
        "Pass.MeshletCull", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .AddSpecialization("REOCCLUSION", Reocclusion)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(meshletCullTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(meshletCullTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

template <bool Reocclusion>
void MeshletCullPassGeneral<Reocclusion>::AddToGraph(RenderGraph::Graph& renderGraph,
    MeshletCullContext& ctx)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

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

    std::string passName = m_Name.Name() + (Reocclusion ? ".Reocclusion" : "");

    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{passName},
        [&](Graph& graph, PassData& passData)
        {
            // if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass
            if constexpr(!Reocclusion)
            {
                ctx.Resources().MeshletsSsbo = graph.AddExternal(std::format("{}.{}", passName, "Meshlets"),
                    ctx.Geometry().GetMeshletsBuffer());
                ctx.Resources().VisibilitySsbo =
                    graph.AddExternal(std::format("{}.{}", passName, "Visibility"), ctx.Visibility());
                ctx.Resources().CommandsSsbo =
                    graph.AddExternal(std::format("{}.{}", passName, "Commands"), ctx.Geometry().GetCommandsBuffer());
                ctx.Resources().CompactCommandsSsbo =
                    graph.CreateResource(std::format("{}.{}", passName, "Commands.Compact"),
                        GraphBufferDescription{.SizeBytes = ctx.Geometry().GetCommandsBuffer().GetSizeBytes()});
                // count buffer will be separate for ordinary and reocclusion passes
                ctx.Resources().CompactCountSsbo =
                    graph.CreateResource(std::format("{}.{}", passName, "Commands.CompactCount"),
                        GraphBufferDescription{.SizeBytes = sizeof(u32)});
                ctx.Resources().CompactCountReocclusionSsbo =
                    graph.CreateResource(std::format("{}.{}", passName, "Commands.CompactCount.Reocclusion"),
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});
            }

            auto& meshResources = ctx.MeshContext().Resources();
            meshResources.HiZ = graph.Read(meshResources.HiZ, Compute | Sampled);
            meshResources.SceneUbo = graph.Read(meshResources.SceneUbo, Compute | Uniform);
            meshResources.ObjectsSsbo = graph.Read(meshResources.ObjectsSsbo, Compute | Storage);
            meshResources.VisibilitySsbo = graph.Read(meshResources.VisibilitySsbo, Compute | Storage);

            auto& resources = ctx.Resources();
            resources.MeshletsSsbo = graph.Read(resources.MeshletsSsbo, Compute | Storage);
            resources.VisibilitySsbo = graph.Read(resources.VisibilitySsbo, Compute | Storage);
            resources.VisibilitySsbo = graph.Write(resources.VisibilitySsbo, Compute | Storage);
            resources.CommandsSsbo = graph.Read(resources.CommandsSsbo, Compute | Storage);
            resources.CompactCommandsSsbo = graph.Read(resources.CompactCommandsSsbo, Compute | Storage);
            resources.CompactCommandsSsbo = graph.Write(resources.CompactCommandsSsbo, Compute | Storage);
            if constexpr(!Reocclusion)
            {
                resources.CompactCountSsbo = graph.Read(resources.CompactCountSsbo, Compute | Storage | Upload);
                resources.CompactCountSsbo = graph.Write(resources.CompactCountSsbo, Compute | Storage);
            }
            else
            {
                resources.CompactCountReocclusionSsbo =
                    graph.Read(resources.CompactCountReocclusionSsbo, Compute | Storage | Upload);
                resources.CompactCountReocclusionSsbo =
                    graph.Write(resources.CompactCountReocclusionSsbo, Compute | Storage);
            }

            passData.MeshResources = meshResources;
            passData.MeshletResources = resources;
            passData.MeshletCount = ctx.Geometry().GetMeshletCount();
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Meshlet Cull")

            auto& meshResources = passData.MeshResources;
            auto& meshletResources = passData.MeshletResources;
            
            const Texture& hiz = resources.GetTexture(meshResources.HiZ);
            const Sampler& hizSampler = meshResources.HiZSampler;
            const Buffer& sceneUbo = resources.GetBuffer(meshResources.SceneUbo);
            const Buffer& objectsSsbo = resources.GetBuffer(meshResources.ObjectsSsbo);
            const Buffer& objectVisibilitySsbo = resources.GetBuffer(meshResources.VisibilitySsbo);
            const Buffer& meshletsSsbo = resources.GetBuffer(meshletResources.MeshletsSsbo);
            const Buffer& meshletVisibilitySsbo = resources.GetBuffer(meshletResources.VisibilitySsbo);
            const Buffer& commandsSsbo = resources.GetBuffer(meshletResources.CommandsSsbo);
            const Buffer& compactCommandsSsbo = resources.GetBuffer(meshletResources.CompactCommandsSsbo);

            const Buffer& countSsbo = resources.GetBuffer(Reocclusion ?
                meshletResources.CompactCountReocclusionSsbo : meshletResources.CompactCountSsbo, 0u,
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

            u32 meshletCount = passData.MeshletCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}



