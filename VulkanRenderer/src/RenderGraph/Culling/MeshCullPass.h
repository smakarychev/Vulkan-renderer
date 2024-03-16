#pragma once

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "RenderGraph/HiZ/HiZPass.h"
#include "Vulkan/RenderCommand.h"

class MeshCullContext
{
public:
    MeshCullContext(const RenderPassGeometry& geometry);

    const Buffer& Visibility() const { return m_Visibility; }
    const RenderPassGeometry& Geometry() const { return *m_Geometry; }
private:
    Buffer m_Visibility{};
    
    const RenderPassGeometry* m_Geometry{nullptr};
};

template <bool Reocclusion>
class MeshCullGeneralPass
{
public:
    struct SceneUBO
    {
        glm::mat4 ViewMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 HiZWidth;
        f32 HiZHeight;
    
        u64 Padding;
    };
    struct PassData
    {
        RenderGraph::Resource HiZ;
        Sampler HiZSampler;
        RenderGraph::Resource SceneUbo;
        RenderGraph::Resource ObjectsSsbo;
        RenderGraph::Resource VisibilitySsbo;
        
        SceneUBO Scene;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    MeshCullGeneralPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, const MeshCullContext& ctx, const HiZPassContext& hiZPassContext);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData;
};


using MeshCullPass = MeshCullGeneralPass<false>;
using MeshCullReocclusionPass = MeshCullGeneralPass<true>;


template <bool Reocclusion>
MeshCullGeneralPass<Reocclusion>::MeshCullGeneralPass(RenderGraph::Graph& renderGraph)
{
    ShaderPipelineTemplate* meshCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/mesh-cull-comp.shader"},
        "render-graph-mesh-cull-pass-template", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshCullTemplate)
        .AddSpecialization("REOCCLUSION", Reocclusion)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(meshCullTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(meshCullTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

template <bool Reocclusion>
void MeshCullGeneralPass<Reocclusion>::AddToGraph(RenderGraph::Graph& renderGraph, const MeshCullContext& ctx,
    const HiZPassContext& hiZPassContext)
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
    static ShaderDescriptors::BindingInfo visibilityBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_object_visibility");

    static constexpr std::string_view PASS_NAME = Reocclusion ? "mesh-cull-reocclusion" : "mesh-cull";
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(std::string{PASS_NAME},
        [&](Graph& graph, PassData& passData)
        {
            // if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass
            if constexpr(!Reocclusion)
            {
                // we do not really need hiz for ordinary pass, but it is still has to be provided
                passData.HiZ = graph.AddExternal("hiz-previous", hiZPassContext.GetHiZPrevious(),
                    ImageUtils::DefaultTexture::Black);

                passData.SceneUbo = graph.CreateResource("mesh-cull-scene", GraphBufferDescription{
                    .SizeBytes = sizeof(SceneUBO)});
                passData.ObjectsSsbo = graph.AddExternal("mesh-cull-objects", ctx.Geometry().GetRenderObjectsBuffer());
                passData.VisibilitySsbo = graph.AddExternal("mesh-cull-object-visibility", ctx.Visibility());
            }
            else
            {
                passData.HiZ = graph.GetBlackboard().GetOutput<HiZPass::PassData>().HiZOut;

                auto& meshOutput = graph.GetBlackboard().GetOutput<MeshCullPass::PassData>();
                passData.SceneUbo = meshOutput.SceneUbo;
                passData.ObjectsSsbo = meshOutput.ObjectsSsbo;
                passData.VisibilitySsbo = meshOutput.VisibilitySsbo;
            }

            passData.HiZSampler = hiZPassContext.GetSampler();
            passData.HiZ = graph.Read(passData.HiZ, Compute | Sampled);
            passData.SceneUbo = graph.Read(passData.SceneUbo, Compute | Uniform | Upload);
            passData.ObjectsSsbo = graph.Read(passData.ObjectsSsbo, Compute | Storage);
            passData.VisibilitySsbo = graph.Read(passData.VisibilitySsbo, Compute | Storage);
            passData.VisibilitySsbo = graph.Write(passData.VisibilitySsbo, Compute |Storage);
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Mesh Cull")
            
            const Texture& hiz = resources.GetTexture(passData.HiZ);
            const Sampler& hizSampler = passData.HiZSampler;
            
            passData.Scene.ViewMatrix = frameContext.Camera->GetView();
            passData.Scene.FrustumPlanes = frameContext.Camera->GetFrustumPlanes();
            passData.Scene.ProjectionData = frameContext.Camera->GetProjectionData();
            passData.Scene.HiZWidth = (f32)hiz.GetDescription().Width;
            passData.Scene.HiZHeight = (f32)hiz.GetDescription().Height;
            const Buffer& sceneUbo = resources.GetBuffer(passData.SceneUbo, passData.Scene,
                *frameContext.ResourceUploader);

            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& visibilitySsbo = resources.GetBuffer(passData.VisibilitySsbo);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding(samplerBinding, hiz.CreateBindingInfo(hizSampler, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(hizBinding, hiz.CreateBindingInfo(hizSampler, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(sceneBinding, sceneUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(objectsBinding, objectsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(visibilityBinding, visibilitySsbo.CreateBindingInfo());

            u32 objectCount = ctx.Geometry().GetRenderObjectCount();

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), objectCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {objectCount, 1, 1}, {64, 1, 1});
        });
}
