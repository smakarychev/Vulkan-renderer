#include "MeshletCullTranslucentPass.h"

#include "MeshletCullPass.h"

MeshletCullTranslucentContext::MeshletCullTranslucentContext(MeshCullContext& meshCullContext)
    : m_MeshCullContext(&meshCullContext)
{
}

MeshletCullTranslucentPass::MeshletCullTranslucentPass(RG::Graph& renderGraph, std::string_view name,
    const MeshletCullPassInitInfo& info)
        : m_Name(name)
{
    ShaderPipelineTemplate* meshletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/meshlet-cull-translucent-comp.shader"},
        "Pass.MeshletCullTranslucent", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .AddSpecialization("IS_ORTHOGRAPHIC_PROJECTION", info.CameraType == CameraType::Orthographic)
        .AddSpecialization("CLAMP_DEPTH", info.ClampDepth)
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

void MeshletCullTranslucentPass::AddToGraph(RG::Graph& renderGraph,
    MeshletCullTranslucentContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    std::string passName = m_Name.Name();
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{passName},
        [&](Graph& graph, PassData& passData)
        {
            ctx.Resources().Meshlets = graph.AddExternal(std::format("{}.{}", passName, "Meshlets"),
                    ctx.Geometry().GetMeshletsBuffer());
            ctx.Resources().Commands =
                graph.AddExternal(std::format("{}.{}", passName, "Commands"), ctx.Geometry().GetCommandsBuffer());

            auto& meshResources = ctx.MeshContext().Resources();
            meshResources.HiZ = graph.Read(meshResources.HiZ, Compute | Sampled);
            meshResources.Scene = graph.Read(meshResources.Scene, Compute | Uniform);
            meshResources.Objects = graph.Read(meshResources.Objects, Compute | Storage);
            meshResources.Visibility = graph.Read(meshResources.Visibility, Compute | Storage);

            auto& resources = ctx.Resources();
            resources.Meshlets = graph.Read(resources.Meshlets, Compute | Storage);
            resources.Commands = graph.Read(resources.Commands, Compute | Storage);
            resources.Commands = graph.Write(resources.Commands, Compute | Storage);

            passData.MeshResources = meshResources;
            passData.MeshletResources = resources;
            passData.MeshletCount = ctx.Geometry().GetMeshletCount();
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Meshlet Translucent Cull")

            auto& meshResources = passData.MeshResources;
            auto& meshletResources = passData.MeshletResources;
            
            const Texture& hiz = resources.GetTexture(meshResources.HiZ);
            const Sampler& hizSampler = meshResources.HiZSampler;
            const Buffer& scene = resources.GetBuffer(meshResources.Scene);
            const Buffer& objects = resources.GetBuffer(meshResources.Objects);
            const Buffer& objectVisibility = resources.GetBuffer(meshResources.Visibility);
            const Buffer& meshlets = resources.GetBuffer(meshletResources.Meshlets);
            const Buffer& commands = resources.GetBuffer(meshletResources.Commands);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", scene.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", objectVisibility.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlets", meshlets.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commands.BindingInfo());

            u32 meshletCount = passData.MeshletCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}
