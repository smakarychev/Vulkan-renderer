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
            ctx.Resources().MeshletsSsbo = graph.AddExternal(std::format("{}.{}", passName, "Meshlets"),
                    ctx.Geometry().GetMeshletsBuffer());
            ctx.Resources().CommandsSsbo =
                graph.AddExternal(std::format("{}.{}", passName, "Commands"), ctx.Geometry().GetCommandsBuffer());

            auto& meshResources = ctx.MeshContext().Resources();
            meshResources.HiZ = graph.Read(meshResources.HiZ, Compute | Sampled);
            meshResources.SceneUbo = graph.Read(meshResources.SceneUbo, Compute | Uniform);
            meshResources.ObjectsSsbo = graph.Read(meshResources.ObjectsSsbo, Compute | Storage);
            meshResources.VisibilitySsbo = graph.Read(meshResources.VisibilitySsbo, Compute | Storage);

            auto& resources = ctx.Resources();
            resources.MeshletsSsbo = graph.Read(resources.MeshletsSsbo, Compute | Storage);
            resources.CommandsSsbo = graph.Read(resources.CommandsSsbo, Compute | Storage);
            resources.CommandsSsbo = graph.Write(resources.CommandsSsbo, Compute | Storage);

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
            const Buffer& sceneUbo = resources.GetBuffer(meshResources.SceneUbo);
            const Buffer& objectsSsbo = resources.GetBuffer(meshResources.ObjectsSsbo);
            const Buffer& objectVisibilitySsbo = resources.GetBuffer(meshResources.VisibilitySsbo);
            const Buffer& meshletsSsbo = resources.GetBuffer(meshletResources.MeshletsSsbo);
            const Buffer& commandsSsbo = resources.GetBuffer(meshletResources.CommandsSsbo);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", sceneUbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", objectVisibilitySsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlets", meshletsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsSsbo.BindingInfo());

            u32 meshletCount = passData.MeshletCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}
