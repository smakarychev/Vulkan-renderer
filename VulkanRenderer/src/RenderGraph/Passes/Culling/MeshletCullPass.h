#pragma once

#include "MeshCullPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGeometry.h"
#include "Rendering/Buffer.h"

class MeshCullContext;

class MeshletCullContext
{
public:
    struct PassResources
    {
        RG::Resource MeshletsSsbo{};
        RG::Resource VisibilitySsbo{};
        RG::Resource CommandsSsbo{};
        RG::Resource CompactCommandsSsbo{};
        RG::Resource CompactCountSsbo{};
        RG::Resource CompactCountReocclusionSsbo{};
    };
public:
    MeshletCullContext(MeshCullContext& meshCullContext);
    void UpdateCompactCounts();
    u32 CompactCountValue() const { return ReadbackCount(m_CompactCount[m_FrameNumber]); }
    u32 CompactCountReocclusionValue() const { return ReadbackCount(m_CompactCountReocclusion[m_FrameNumber]); }

    const Buffer& Visibility() const { return m_Visibility; }
    const Buffer& CompactCount() const { return m_CompactCount[m_FrameNumber]; }
    const Buffer& CompactCountReocclusion() const { return m_CompactCountReocclusion[m_FrameNumber]; }
    const RG::Geometry& Geometry() const { return m_MeshCullContext->Geometry(); }
    MeshCullContext& MeshContext() const { return *m_MeshCullContext; }
    PassResources& Resources() { return m_Resources; }
private:
    u32 ReadbackCount(const Buffer& buffer) const;
private:
    Buffer m_Visibility;

    /* can be detached from real frame number */
    u32 m_FrameNumber{0};
    std::array<Buffer, BUFFERED_FRAMES> m_CompactCount;
    std::array<Buffer, BUFFERED_FRAMES> m_CompactCountReocclusion;
    std::array<u32, BUFFERED_FRAMES> m_CompactCountValues{};
    std::array<u32, BUFFERED_FRAMES> m_CompactCountReocclusionValues{};

    MeshCullContext* m_MeshCullContext{nullptr};
    PassResources m_Resources{};
};

template <CullStage Stage>
class MeshletCullPassGeneral
{
public:
    struct PassData
    {
        MeshCullContext::PassResources MeshResources;
        MeshletCullContext::PassResources MeshletResources;

        u32 MeshletCount;
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    MeshletCullPassGeneral(RG::Graph& renderGraph, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, MeshletCullContext& ctx);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};


using MeshletCullPass = MeshletCullPassGeneral<CullStage::Cull>;
using MeshletCullReocclusionPass = MeshletCullPassGeneral<CullStage::Reocclusion>;
using MeshletCullSinglePass = MeshletCullPassGeneral<CullStage::Single>;


template <CullStage Stage>
MeshletCullPassGeneral<Stage>::MeshletCullPassGeneral(RG::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* meshletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/meshlet-cull-comp.shader"},
        "Pass.MeshletCull", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .AddSpecialization("REOCCLUSION", Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", Stage == CullStage::Single)
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

template <CullStage Stage>
void MeshletCullPassGeneral<Stage>::AddToGraph(RG::Graph& renderGraph,
    MeshletCullContext& ctx)
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
                    graph.AddExternal(std::format("{}.{}", passName, "Commands.CompactCount"), ctx.CompactCount());
                ctx.Resources().CompactCountReocclusionSsbo =
                    graph.AddExternal(std::format("{}.{}", passName, "Commands.CompactCount.Reocclusion"),
                        ctx.CompactCountReocclusion());
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
            if constexpr(Stage != CullStage::Reocclusion)
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

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
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

            const Buffer& countSsbo = resources.GetBuffer(Stage == CullStage::Reocclusion ?
                meshletResources.CompactCountReocclusionSsbo : meshletResources.CompactCountSsbo, 0u,
                *frameContext.ResourceUploader);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", sceneUbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", objectVisibilitySsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlets", meshletsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlet_visibility", meshletVisibilitySsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_compacted_commands", compactCommandsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_count", countSsbo.BindingInfo());

            u32 meshletCount = passData.MeshletCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}