#pragma once

#include "MeshCullPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "Scene/SceneGeometry.h"
#include "Rendering/Buffer.h"

class MeshCullContext;

class MeshletCullContext
{
public:
    struct PassResources
    {
        RG::Resource Meshlets{};
        RG::Resource Visibility{};
        RG::Resource Commands{};
        RG::Resource CompactCommands{};
        /* count buffer will be separate for ordinary and reocclusion passes */
        RG::Resource CompactCount{};
        RG::Resource CompactCountReocclusion{};

        /* is used to mark commands that were processed by triangle culling */
        RG::Resource CommandFlags{};
    };
public:
    MeshletCullContext(MeshCullContext& meshCullContext);
    /* should be called once a frame */
    void NextFrame() { m_FrameNumber = (m_FrameNumber + 1) % BUFFERED_FRAMES; }
    u32 ReadbackCompactCountValue();
    u32 CompactCountValue() const { return m_CompactCountValue; }

    const Buffer& Visibility() const { return m_Visibility; }
    const Buffer& CompactCount() const { return m_CompactCount[m_FrameNumber]; }
    const SceneGeometry& Geometry() const { return m_MeshCullContext->Geometry(); }
    MeshCullContext& MeshContext() const { return *m_MeshCullContext; }
    PassResources& Resources() { return m_Resources; }
    const PassResources& Resources() const { return m_Resources; }
private:
    u32 ReadbackCount(const Buffer& buffer) const;
    u32 PreviousFrame() const { return (m_FrameNumber + BUFFERED_FRAMES - 1) % BUFFERED_FRAMES; }
private:
    Buffer m_Visibility;

    /* can be detached from real frame number */
    u32 m_FrameNumber{0};
    std::array<Buffer, BUFFERED_FRAMES> m_CompactCount;
    std::array<u32, BUFFERED_FRAMES> m_CompactCountReocclusionValues{};

    u32 m_CompactCountValue{0};

    MeshCullContext* m_MeshCullContext{nullptr};
    PassResources m_Resources{};
};

struct MeshletCullPassInitInfo
{
    bool ClampDepth{false};
    CameraType CameraType{CameraType::Perspective};
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
    MeshletCullPassGeneral(RG::Graph& renderGraph, std::string_view name, const MeshletCullPassInitInfo& info);
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
MeshletCullPassGeneral<Stage>::MeshletCullPassGeneral(RG::Graph& renderGraph, std::string_view name,
    const MeshletCullPassInitInfo& info)
        : m_Name(name)
{
    ShaderPipelineTemplate* meshletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/meshlet-cull-comp.shader"},
        "Pass.MeshletCull", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .AddSpecialization("REOCCLUSION", Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", Stage == CullStage::Single)
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
            /* if it is an ordinary pass, create buffers, otherwise, use buffers of ordinary pass */
            if constexpr(Stage != CullStage::Reocclusion)
            {
                ctx.Resources().Meshlets = graph.AddExternal(std::format("{}.{}", passName, "Meshlets"),
                    ctx.Geometry().GetMeshletsBuffer());
                ctx.Resources().Visibility = graph.AddExternal(
                    std::format("{}.{}", passName, "Visibility"), ctx.Visibility());
                ctx.Resources().Commands = graph.AddExternal(
                    std::format("{}.{}", passName, "Commands"), ctx.Geometry().GetCommandsBuffer());
                ctx.Resources().CompactCommands = graph.CreateResource(
                    std::format("{}.{}", passName, "Commands.Compact"),
                    GraphBufferDescription{.SizeBytes = ctx.Geometry().GetCommandsBuffer().GetSizeBytes()});
                ctx.Resources().CompactCount = graph.AddExternal(
                    std::format("{}.{}", passName, "Commands.CompactCount"), ctx.CompactCount());
                ctx.Resources().CompactCountReocclusion = graph.CreateResource(
                    std::format("{}.{}", passName, "Commands.CompactCount.Reocclusion"),
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});

                ctx.Resources().CommandFlags = graph.CreateResource(
                    std::format("{}.{}", passName, "Commands.CommandFlags"),
                    GraphBufferDescription{.SizeBytes = ctx.Geometry().GetMeshletCount() * sizeof(u8)});
            }

            auto& meshResources = ctx.MeshContext().Resources();
            meshResources.HiZ = graph.Read(meshResources.HiZ, Compute | Sampled);
            meshResources.Scene = graph.Read(meshResources.Scene, Compute | Uniform);
            meshResources.Objects = graph.Read(meshResources.Objects, Compute | Storage);
            meshResources.Visibility = graph.Read(meshResources.Visibility, Compute | Storage);

            auto& resources = ctx.Resources();
            resources.Meshlets = graph.Read(resources.Meshlets, Compute | Storage);
            resources.Visibility = graph.Read(resources.Visibility, Compute | Storage);
            resources.Visibility = graph.Write(resources.Visibility, Compute | Storage);
            resources.Commands = graph.Read(resources.Commands, Compute | Storage);
            resources.CompactCommands = graph.Read(resources.CompactCommands, Compute | Storage);
            resources.CompactCommands = graph.Write(resources.CompactCommands, Compute | Storage);
            if constexpr(Stage != CullStage::Reocclusion)
            {
                resources.CompactCount = graph.Read(resources.CompactCount, Compute | Storage | Upload);
                resources.CompactCount = graph.Write(resources.CompactCount, Compute | Storage);
            }
            else
            {
                resources.CompactCountReocclusion =
                    graph.Read(resources.CompactCountReocclusion, Compute | Storage | Upload);
                resources.CompactCountReocclusion =
                    graph.Write(resources.CompactCountReocclusion, Compute | Storage);

                resources.CommandFlags = graph.Read(resources.CommandFlags, Compute | Storage);    
            }
            resources.CommandFlags = graph.Write(resources.CommandFlags, Compute | Storage);

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
            const Buffer& scene = resources.GetBuffer(meshResources.Scene);
            const Buffer& objects = resources.GetBuffer(meshResources.Objects);
            const Buffer& objectVisibility = resources.GetBuffer(meshResources.Visibility);
            const Buffer& meshlets = resources.GetBuffer(meshletResources.Meshlets);
            const Buffer& meshletVisibility = resources.GetBuffer(meshletResources.Visibility);
            const Buffer& commands = resources.GetBuffer(meshletResources.Commands);
            const Buffer& compactCommands = resources.GetBuffer(meshletResources.CompactCommands);

            const Buffer& count = resources.GetBuffer(Stage == CullStage::Reocclusion ?
                meshletResources.CompactCountReocclusion : meshletResources.CompactCount, 0u,
                *frameContext.ResourceUploader);

            const Buffer& flags = resources.GetBuffer(passData.MeshletResources.CommandFlags);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_scene_data", scene.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_object_visibility", objectVisibility.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlets", meshlets.BindingInfo());
            resourceDescriptors.UpdateBinding("u_meshlet_visibility", meshletVisibility.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commands.BindingInfo());
            resourceDescriptors.UpdateBinding("u_compacted_commands", compactCommands.BindingInfo());
            resourceDescriptors.UpdateBinding("u_count", count.BindingInfo());
            resourceDescriptors.UpdateBinding("u_flags", flags.BindingInfo());

            u32 meshletCount = passData.MeshletCount;

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), meshletCount);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {meshletCount, 1, 1}, {64, 1, 1});
        });
}