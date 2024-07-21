#pragma once

#include "CullingTraits.h"
#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/Passes/HiZ/HiZPass.h"
#include "Vulkan/RenderCommand.h"

class MeshCullContext
{
public:
    struct PassResources
    {
        RG::Resource HiZ{};
        Sampler HiZSampler{};
        RG::Resource Scene{};
        RG::Resource Objects{};
        RG::Resource Visibility{};
    };
public:
    MeshCullContext(const SceneGeometry& geometry);

    void SetCamera(const Camera* camera) { m_Camera = camera; }
    const Camera& GetCamera() const { return *m_Camera; }

    const Buffer& Visibility() { return m_Visibility; }
    const SceneGeometry& Geometry() { return *m_Geometry; }
    PassResources& Resources() { return m_Resources; }
private:
    const Camera* m_Camera{nullptr};
    
    Buffer m_Visibility{};
    
    const SceneGeometry* m_Geometry{nullptr};
    PassResources m_Resources{};
};

struct MeshCullPassInitInfo
{
    bool ClampDepth{false};
};

template <CullStage Stage>
class MeshCullGeneralPass
{
public:
    struct SceneUBO
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ViewProjectionMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 HiZWidth;
        f32 HiZHeight;
    };
    struct PassData
    {
        MeshCullContext::PassResources Resources{};
        u32 ObjectCount;
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    MeshCullGeneralPass(RG::Graph& renderGraph, std::string_view name, const MeshCullPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, MeshCullContext& ctx, const HiZPassContext& hiZPassContext);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};


using MeshCullPass = MeshCullGeneralPass<CullStage::Cull>;
using MeshCullReocclusionPass = MeshCullGeneralPass<CullStage::Reocclusion>;
using MeshCullSinglePass = MeshCullGeneralPass<CullStage::Single>;


template <CullStage Stage>
MeshCullGeneralPass<Stage>::MeshCullGeneralPass(RG::Graph& renderGraph, std::string_view name,
    const MeshCullPassInitInfo& info)
        : m_Name(name)
{
    ShaderPipelineTemplate* meshCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/mesh-cull-comp.stage"},
        "Pass.MeshCull", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshCullTemplate)
        .AddSpecialization("REOCCLUSION", Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", Stage == CullStage::Single)
        .AddSpecialization("CLAMP_DEPTH", info.ClampDepth)
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
void MeshCullGeneralPass<Stage>::AddToGraph(RG::Graph& renderGraph, MeshCullContext& ctx,
    const HiZPassContext& hiZPassContext)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    std::string passName = m_Name.Name() + cullStageToString(Stage);
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{passName},
        [&](Graph& graph, PassData& passData)
        {
            // if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass
            if constexpr(Stage != CullStage::Reocclusion)
            {
                ctx.Resources().Scene = graph.CreateResource(std::format("{}.{}", passName, "Scene"),
                    GraphBufferDescription{.SizeBytes = sizeof(SceneUBO)});
                ctx.Resources().Objects =
                    graph.AddExternal(std::format("{}.{}", passName, "Objects"),
                        ctx.Geometry().GetRenderObjectsBuffer());
                ctx.Resources().Visibility =
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
                ctx.Resources().HiZ = hiZPassContext.GetHiZResource();
            }

            auto& resources = ctx.Resources();
            resources.HiZSampler = hiZPassContext.GetSampler();
            resources.HiZ = graph.Read(resources.HiZ, Compute | Sampled);
            resources.Scene = graph.Read(resources.Scene, Compute | Uniform | Upload);
            resources.Objects = graph.Read(resources.Objects, Compute | Storage);
            resources.Visibility = graph.Read(resources.Visibility, Compute | Storage);
            resources.Visibility = graph.Write(resources.Visibility, Compute |Storage);

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
            scene.ViewMatrix = ctx.GetCamera().GetView();
            scene.ViewProjectionMatrix = ctx.GetCamera().GetViewProjection();
            scene.FrustumPlanes = ctx.GetCamera().GetFrustumPlanes();
            scene.ProjectionData = ctx.GetCamera().GetProjectionData();
            scene.HiZWidth = (f32)hiz.Description().Width;
            scene.HiZHeight = (f32)hiz.Description().Height;
            const Buffer& sceneBuffer = resources.GetBuffer(passData.Resources.Scene, scene,
                *frameContext.ResourceUploader);

            const Buffer& objects = resources.GetBuffer(passData.Resources.Objects);
            const Buffer& visibility = resources.GetBuffer(passData.Resources.Visibility);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", sceneBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", visibility.BindingInfo());

            u32 objectCount = passData.ObjectCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), objectCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {objectCount, 1, 1}, {64, 1, 1});
        });
}
