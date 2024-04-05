#pragma once

#include "CullingTraits.h"
#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGeometry.h"
#include "RenderGraph/Passes/HiZ/HiZPass.h"
#include "Vulkan/RenderCommand.h"

class MeshCullContext
{
public:
    struct PassResources
    {
        RenderGraph::Resource HiZ{};
        Sampler HiZSampler{};
        RenderGraph::Resource SceneUbo{};
        RenderGraph::Resource ObjectsSsbo{};
        RenderGraph::Resource VisibilitySsbo{};
    };
public:
    MeshCullContext(const RenderPassGeometry& geometry);

    const Buffer& Visibility() { return m_Visibility; }
    const RenderPassGeometry& Geometry() { return *m_Geometry; }
    PassResources& Resources() { return m_Resources; }
private:
    Buffer m_Visibility{};
    
    const RenderPassGeometry* m_Geometry{nullptr};
    PassResources m_Resources{};
};

template <CullStage Stage>
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
        MeshCullContext::PassResources Resources{};
        u32 ObjectCount;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    MeshCullGeneralPass(RenderGraph::Graph& renderGraph, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, MeshCullContext& ctx, const HiZPassContext& hiZPassContext);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::PipelineData m_PipelineData;
};


using MeshCullPass = MeshCullGeneralPass<CullStage::Cull>;
using MeshCullReocclusionPass = MeshCullGeneralPass<CullStage::Reocclusion>;
using MeshCullSinglePass = MeshCullGeneralPass<CullStage::Single>;


template <CullStage Stage>
MeshCullGeneralPass<Stage>::MeshCullGeneralPass(RenderGraph::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* meshCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/mesh-cull-comp.shader"},
        "Pass.MeshCull", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshCullTemplate)
        .AddSpecialization("REOCCLUSION", Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", Stage == CullStage::Single)
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

template <CullStage Stage>
void MeshCullGeneralPass<Stage>::AddToGraph(RenderGraph::Graph& renderGraph, MeshCullContext& ctx,
    const HiZPassContext& hiZPassContext)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;
    
    std::string passName = m_Name.Name() + cullStageToString(Stage);
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{passName},
        [&](Graph& graph, PassData& passData)
        {
            // if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass
            if constexpr(Stage != CullStage::Reocclusion)
            {

                ctx.Resources().SceneUbo = graph.CreateResource(std::format("{}.{}", passName, "Scene"),
                    GraphBufferDescription{.SizeBytes = sizeof(SceneUBO)});
                ctx.Resources().ObjectsSsbo =
                    graph.AddExternal(std::format("{}.{}", passName, "Objects"),
                        ctx.Geometry().GetRenderObjectsBuffer());
                ctx.Resources().VisibilitySsbo =
                    graph.AddExternal(std::format("{}.{}", passName, "Visibility"), ctx.Visibility());
            }
            if constexpr(Stage == CullStage::Cull)
            {
                // we do not really need hiz for ordinary pass, but it is still has to be provided
                ctx.Resources().HiZ = graph.AddExternal("Hiz.Previous", hiZPassContext.GetHiZPrevious(),
                    ImageUtils::DefaultTexture::Black);
            }
            else
            {
                ctx.Resources().HiZ = graph.GetBlackboard().Get<HiZPass::PassData>().HiZOut;
            }

            auto& resources = ctx.Resources();
            resources.HiZSampler = hiZPassContext.GetSampler();
            resources.HiZ = graph.Read(resources.HiZ, Compute | Sampled);
            resources.SceneUbo = graph.Read(resources.SceneUbo, Compute | Uniform | Upload);
            resources.ObjectsSsbo = graph.Read(resources.ObjectsSsbo, Compute | Storage);
            resources.VisibilitySsbo = graph.Read(resources.VisibilitySsbo, Compute | Storage);
            resources.VisibilitySsbo = graph.Write(resources.VisibilitySsbo, Compute |Storage);

            passData.Resources = resources;
            passData.ObjectCount = ctx.Geometry().GetRenderObjectCount();
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Mesh Cull")
            
            const Texture& hiz = resources.GetTexture(passData.Resources.HiZ);
            const Sampler& hizSampler = passData.Resources.HiZSampler;

            SceneUBO scene = {};
            scene.ViewMatrix = frameContext.MainCamera->GetView();
            scene.FrustumPlanes = frameContext.MainCamera->GetFrustumPlanes();
            scene.ProjectionData = frameContext.MainCamera->GetProjectionData();
            scene.HiZWidth = (f32)hiz.GetDescription().Width;
            scene.HiZHeight = (f32)hiz.GetDescription().Height;
            const Buffer& sceneUbo = resources.GetBuffer(passData.Resources.SceneUbo, scene,
                *frameContext.ResourceUploader);

            const Buffer& objectsSsbo = resources.GetBuffer(passData.Resources.ObjectsSsbo);
            const Buffer& visibilitySsbo = resources.GetBuffer(passData.Resources.VisibilitySsbo);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", sceneUbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", visibilitySsbo.BindingInfo());

            u32 objectCount = passData.ObjectCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), objectCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {objectCount, 1, 1}, {64, 1, 1});
        });
}
