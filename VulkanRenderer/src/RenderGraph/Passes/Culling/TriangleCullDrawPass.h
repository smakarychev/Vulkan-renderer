#pragma once

#include "MeshletCullPass.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGeometry.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/HiZ/HiZPass.h"
#include "Rendering/RenderingUtils.h"

class TriangleCullContext
{
public:
    static constexpr u32 MAX_BATCHES = 2;
    static constexpr u32 MAX_TRIANGLES = 128'000;
    static constexpr u32 MAX_INDICES = MAX_TRIANGLES * 3;
    static constexpr u32 MAX_COMMANDS = MAX_TRIANGLES / assetLib::ModelInfo::TRIANGLES_PER_MESHLET;
    using TriangleType = u8;
    using IndexType = u32;
    struct PassResources
    {
        RG::Resource SceneUbo{};
        RG::Resource TriangleVisibilitySsbo{};
        RG::Resource IndicesSsbo{};
        RG::Resource DispatchIndirect{};

        std::array<RG::Resource, MAX_BATCHES> TrianglesSsbo{};
        std::array<RG::Resource, MAX_BATCHES> IndicesCulledSsbo{};
        std::array<RG::Resource, MAX_BATCHES> IndicesCulledCountSsbo{};
        std::array<RG::Resource, MAX_BATCHES> DrawIndirect{};
    };
public:
    TriangleCullContext(MeshletCullContext& meshletCullContext);
    
    static u32 GetTriangleCount()
    {
        return MAX_TRIANGLES * SUB_BATCH_COUNT;
    }
    static u32 GetIndexCount()
    {
        return MAX_INDICES * SUB_BATCH_COUNT;
    }
    static u32 GetCommandCount()
    {
        return MAX_COMMANDS * SUB_BATCH_COUNT;
    }

    const Buffer& Visibility() { return m_Visibility; }
    const RG::Geometry& Geometry() { return m_MeshletCullContext->Geometry(); }
    MeshletCullContext& MeshletContext() { return *m_MeshletCullContext; }
    PassResources& Resources() { return m_Resources; }

    u32 Iteration() const { return m_Iteration; }
    void NextIteration() { m_Iteration++; }
    void ResetIteration() { m_Iteration = 0; }

    void SetIterationCount(u32 count) { m_IterationCount = count; }
    u32 GetIterationCount() const { return m_IterationCount; }
    
private:
    Buffer m_Visibility;

    u32 m_Iteration{0};
    u32 m_IterationCount{0};
    
    MeshletCullContext* m_MeshletCullContext{nullptr};
    PassResources m_Resources{};
};

struct TriangleCullDrawPassInitInfo
{
    RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};
    std::optional<ShaderDescriptors> MaterialDescriptors{};
    ShaderPipeline DrawPipeline{};
};

class TriangleDrawContext
{
public:
    struct PassResources
    {
        RG::Resource CameraUbo{};
        RG::Resource PositionsSsbo{};
        RG::Resource NormalsSsbo{};
        RG::Resource TangentsSsbo{};
        RG::Resource UVsSsbo{};
        RG::Resource ObjectsSsbo{};
        RG::Resource CommandsSsbo{};

        std::optional<RG::IBLData> IBL{};
        std::optional<RG::SSAOData> SSAO{};

        std::vector<RG::Resource> RenderTargets{};
        std::optional<RG::Resource> DepthTarget{};
    };
public:
    PassResources& Resources() { return m_Resources; }
private:
    PassResources m_Resources{};
};

struct TriangleCullDrawPassExecutionInfo
{
    RG::Resource Dispatch{};
    
    TriangleCullContext* CullContext{nullptr};
    TriangleDrawContext* DrawContext{nullptr};
    HiZPassContext* HiZContext;
    glm::uvec2 Resolution;

    struct Attachment
    {
        RG::Resource Resource{};
        RenderingAttachmentDescription Description{};
    };
    std::vector<Attachment> ColorAttachments;
    std::optional<Attachment> DepthAttachment;

    std::optional<RG::IBLData> IBL{};
    std::optional<RG::SSAOData> SSAO{};
};

template <CullStage Stage>
class TriangleCullPrepareDispatchPass
{
public:
    struct PassData
    {
        RG::Resource CompactCountSsbo;
        RG::Resource DispatchIndirect;
        RG::Resource VisibleMeshletCountSsbo;
        u32 MaxDispatches{0};

        RG::PipelineData* PipelineData{nullptr};
        TriangleCullContext* Context{nullptr};
    };
public:
    TriangleCullPrepareDispatchPass(RG::Graph& renderGraph, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, TriangleCullContext& ctx);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    
    RG::PipelineData m_PipelineData;
};

/* This pass shows a limitation of render graph implementation:
 * the way triangle culling works is that we need to know how many 'batches' there are;
 * it is necessary to cull-draw triangles in batches, because after triangle culling
 * all the meshes that previously shared an index buffer, now have different buffers.
 * To keep memory usage predictable, it is therefore necessary to cull-draw in fixed-sized batches.
 * The count of batches is determined by previous passes, so conceptually we want to do the following:
 * - pre-triangle culling passes (the batch count is now known)
 * - for each batch:
 *      - cull batch
 *      - draw batch
 *      
 * This for loop is problematic: we ultimately want cull pass to be separated from draw pass,
 * which means that we either:
 * 1) call `AddToGraph` BachCount times, or
 * 2) do the work in `ExecutionCallback` BachCount times;
 *
 * both of these options are impossible:
 * - option 2) is impossible because cull and draw are interleaved, and
 * - option 1) is impossible because the recording (`AddToGraph`) happens before the batch count is known.
 *
 * Therefore cull and draw have to be united in this single not so pretty pass.
 */
template <CullStage Stage>
class TriangleCullDrawPass
{
public:
    struct SceneUBO
    {
        glm::mat4 ViewProjectionMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 HiZWidth;
        f32 HiZHeight;
    
        u64 Padding;
    };
    struct PassData
    {
        std::vector<RG::Resource> RenderTargets{};
        std::optional<RG::Resource> DepthTarget{};
    };
public:
    TriangleCullDrawPass(RG::Graph& renderGraph, const TriangleCullDrawPassInitInfo& info,
        std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, const TriangleCullDrawPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    struct PassDataPrivate
    {
        RG::Resource HiZ;
        Sampler HiZSampler;
        RG::Resource ObjectsSsbo;
        RG::Resource MeshletVisibilitySsbo;
        RG::Resource CompactCommandsSsbo;
        RG::Resource CompactCountSsbo;
        
        TriangleCullContext::PassResources TriangleCullResources;
        TriangleDrawContext::PassResources TriangleDrawResources;

        std::array<RG::Resource, TriangleCullContext::MAX_BATCHES> DrawIndirect;

        std::array<RG::PipelineData, TriangleCullContext::MAX_BATCHES>* CullPipelines{nullptr};
        std::array<RG::PipelineData, TriangleCullContext::MAX_BATCHES>* PreparePipelines{nullptr};
        std::array<RG::BindlessTexturesPipelineData, TriangleCullContext::MAX_BATCHES>* DrawPipelines{nullptr};

        RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};

        std::array<SplitBarrier, TriangleCullContext::MAX_BATCHES>* SplitBarriers{nullptr};
        DependencyInfo* SplitBarrierDependency{nullptr};
    };
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    std::array<RG::PipelineData, TriangleCullContext::MAX_BATCHES> m_CullPipelines;
    std::array<RG::PipelineData, TriangleCullContext::MAX_BATCHES> m_PreparePipelines;
    std::array<RG::BindlessTexturesPipelineData, TriangleCullContext::MAX_BATCHES> m_DrawPipelines;

    RG::DrawFeatures m_Features{RG::DrawFeatures::AllAttributes};
    
    std::array<SplitBarrier, TriangleCullContext::MAX_BATCHES> m_SplitBarriers{};
    DependencyInfo m_SplitBarrierDependency{};
};

template <CullStage Stage>
TriangleCullPrepareDispatchPass<Stage>::TriangleCullPrepareDispatchPass(
    RG::Graph& renderGraph, std::string_view name)
        : m_Name(name)
{
    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/prepare-indirect-dispatches-comp.shader"},
        "render-graph-prepare-triangle-cull-pass-template", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(prepareDispatchTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

template <CullStage Stage>
void TriangleCullPrepareDispatchPass<Stage>::AddToGraph(RG::Graph& renderGraph,
    TriangleCullContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo countBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_command_count"); 
    static ShaderDescriptors::BindingInfo dispatchBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_indirect_dispatch"); 

    std::string name = m_Name.Name() + cullStageToString(Stage);
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.MaxDispatches = ctx.Geometry().GetCommandCount() / TriangleCullContext::GetCommandCount() + 1;
            auto& meshletResources = ctx.MeshletContext().Resources();
            passData.CompactCountSsbo = graph.Read(meshletResources.CompactCountSsbo, Compute | Storage);
            ctx.Resources().DispatchIndirect = graph.CreateResource(std::format("{}.{}", name, "Dispatch"),
                GraphBufferDescription{.SizeBytes = renderUtils::alignUniformBufferSizeBytes(
                    passData.MaxDispatches * sizeof(IndirectDispatchCommand))});
            ctx.Resources().DispatchIndirect = graph.Write(ctx.Resources().DispatchIndirect, Compute | Storage);

            if constexpr(Stage != CullStage::Reocclusion)
            {
                ctx.MeshletContext().Resources().CompactCountSsbo =
                    graph.Read(ctx.MeshletContext().Resources().CompactCountSsbo, Readback);
                passData.VisibleMeshletCountSsbo = ctx.MeshletContext().Resources().CompactCountSsbo;
            }
            else
            {
                ctx.MeshletContext().Resources().CompactCountReocclusionSsbo = 
                    graph.Read(ctx.MeshletContext().Resources().CompactCountReocclusionSsbo, Readback);
                passData.VisibleMeshletCountSsbo = ctx.MeshletContext().Resources().CompactCountReocclusionSsbo;
            }
            
            passData.DispatchIndirect = ctx.Resources().DispatchIndirect;
            passData.PipelineData = &m_PipelineData;
            passData.Context = &ctx;

            graph.GetBlackboard().Register(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Prepare Dispatch Indirect")

            const Buffer& countSsbo = resources.GetBuffer(passData.CompactCountSsbo);
            const Buffer& dispatchSsbo = resources.GetBuffer(passData.DispatchIndirect);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(countBinding, countSsbo.BindingInfo());          
            resourceDescriptors.UpdateBinding(dispatchBinding, dispatchSsbo.BindingInfo());

            struct PushConstants
            {
                u32 CommandsPerBatchCount;
                u32 CommandsMultiplier;
                u32 LocalGroupX;
                u32 MaxDispatchesCount;
            };
            PushConstants pushConstants = {
                .CommandsPerBatchCount = TriangleCullContext::GetCommandCount(),
                .CommandsMultiplier = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .LocalGroupX = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .MaxDispatchesCount = passData.MaxDispatches};
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {pushConstants.MaxDispatchesCount, 1, 1}, {64, 1, 1});

            // todo: fix me! this is bad! please!
            // readback cull iteration count
            resources.GetGraph()->OnCmdEnd(frameContext);
            cmd.End();
            Fence readbackFence = Fence::Builder().BuildManualLifetime();
            cmd.Submit(Driver::GetDevice().GetQueues().Graphics, readbackFence);
            readbackFence.Wait();
            Fence::Destroy(readbackFence);
            cmd.Begin();
            resources.GetGraph()->OnCmdBegin(frameContext);

            const Buffer& visibleMeshlets = resources.GetBuffer(passData.VisibleMeshletCountSsbo);
            const void* address = Driver::MapBuffer(visibleMeshlets);
            u32 visibleMeshletsValue = *(u32*)(address);
            u32 commandCount = TriangleCullContext::GetCommandCount();
            u32 iterationCount = visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0);
            Driver::UnmapBuffer(visibleMeshlets);
            passData.Context->SetIterationCount(iterationCount);
            passData.Context->ResetIteration();
        });
}

template <CullStage Stage>
TriangleCullDrawPass<Stage>::TriangleCullDrawPass(RG::Graph& renderGraph,
    const TriangleCullDrawPassInitInfo& info, std::string_view name)
        : m_Name(name)
{
    ShaderPipelineTemplate* triangleCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/triangle-cull-comp.shader"},
        "Pass.TriangleCull", renderGraph.GetArenaAllocators());

    ShaderPipelineTemplate* trianglePrepareDrawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/prepare-draw-comp.shader"},
        "Pass.TriangleCull.PrepareDraw", renderGraph.GetArenaAllocators());

    ASSERT(!info.MaterialDescriptors.has_value() ||
        enumHasAll(info.DrawFeatures,
            RG::DrawFeatures::Materials |
            RG::DrawFeatures::Textures),
        "If 'MaterialDescriptors' are provided, the 'DrawFeatures' must include 'Materials' and 'Textures'")

    ShaderDescriptors immutableSamplers = {};
    if (info.MaterialDescriptors.has_value())
        immutableSamplers = ShaderDescriptors::Builder()
            .SetTemplate(info.DrawPipeline.GetTemplate(), DescriptorAllocatorKind::Samplers)
            .ExtractSet(0)
            .Build();
    
    for (u32 i = 0; i < TriangleCullContext::MAX_BATCHES; i++)
    {
        m_CullPipelines[i].Pipeline = ShaderPipeline::Builder()
            .SetTemplate(triangleCullTemplate)
            .AddSpecialization("REOCCLUSION", Stage == CullStage::Reocclusion)
            .AddSpecialization("SINGLE_PASS", Stage == CullStage::Single)
            .UseDescriptorBuffer()
            .Build();

        m_CullPipelines[i].SamplerDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(triangleCullTemplate, DescriptorAllocatorKind::Samplers)
            .ExtractSet(0)
            .Build();

        m_CullPipelines[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(triangleCullTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();

        m_PreparePipelines[i].Pipeline = ShaderPipeline::Builder()
            .SetTemplate(trianglePrepareDrawTemplate)
            .UseDescriptorBuffer()
            .Build();

        m_PreparePipelines[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(trianglePrepareDrawTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();

        m_DrawPipelines[i].Pipeline = info.DrawPipeline;

        m_DrawPipelines[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(info.DrawPipeline.GetTemplate(), DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();
        
        if (info.MaterialDescriptors.has_value())
        {
            m_DrawPipelines[i].MaterialDescriptors = *info.MaterialDescriptors;
            m_DrawPipelines[i].ImmutableSamplerDescriptors = immutableSamplers;
        }
    }

    m_Features = info.DrawFeatures;

    for (auto& splitBarrier : m_SplitBarriers)
        splitBarrier = SplitBarrier::Builder().Build();
    m_SplitBarrierDependency = DependencyInfo::Builder()
        .MemoryDependency({
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::PixelShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadStorage})
        .Build();
}

template <CullStage Stage>
void TriangleCullDrawPass<Stage>::AddToGraph(RG::Graph& renderGraph,
    const TriangleCullDrawPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    m_Pass = &renderGraph.AddRenderPass<PassDataPrivate>(m_Name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            auto& ctx = *info.CullContext;
            if constexpr(Stage != CullStage::Reocclusion)
            {
                ctx.Resources().SceneUbo = graph.CreateResource(std::format("{}.{}", m_Name.Name(), "Scene"),
                       GraphBufferDescription{.SizeBytes = sizeof(SceneUBO)});
                
                ctx.Resources().TriangleVisibilitySsbo = graph.AddExternal(
                    std::format("{}.{}", m_Name.Name(), "Visibility"), ctx.Visibility());
                ctx.Resources().IndicesSsbo = graph.AddExternal(std::format("{}.{}", m_Name.Name(), "Indices"),
                   ctx.Geometry().GetAttributeBuffers().Indices);

                for (u32 i = 0; i < ctx.MAX_BATCHES; i++)
                {
                    std::string name = std::format("{}.{}", m_Name.Name(), i);

                    ctx.Resources().TrianglesSsbo[i] = graph.CreateResource(
                        std::format("{}.{}", m_Name.Name(), "Triangles"),
                            GraphBufferDescription{
                                .SizeBytes =
                                    TriangleCullContext::GetTriangleCount() *
                                        sizeof(TriangleCullContext::TriangleType)});
                    
                    ctx.Resources().IndicesCulledSsbo[i] =
                        graph.CreateResource(std::format("{}.{}", name, "Indices.Culled"),
                            GraphBufferDescription{
                                .SizeBytes =
                                    TriangleCullContext::GetIndexCount() * sizeof(TriangleCullContext::IndexType)});
                    
                    ctx.Resources().IndicesCulledCountSsbo[i] =
                        graph.CreateResource(std::format("{}.{}", name, "CulledCount"),
                            GraphBufferDescription{.SizeBytes = sizeof(u32)});

                    ctx.Resources().DrawIndirect[i] =
                        graph.CreateResource(std::format("{}.{}", name, "DrawIndirect"),
                            GraphBufferDescription{.SizeBytes = sizeof(IndirectDrawCommand)});
                }

                // draw subpass data
                info.DrawContext->Resources().PositionsSsbo = graph.AddExternal(m_Name.Name() + ".Positions",
                   ctx.Geometry().GetAttributeBuffers().Positions);
                info.DrawContext->Resources().NormalsSsbo = graph.AddExternal(m_Name.Name() + ".Normals",
                    ctx.Geometry().GetAttributeBuffers().Normals);
                info.DrawContext->Resources().TangentsSsbo = graph.AddExternal(m_Name.Name() + ".Tangents",
                    ctx.Geometry().GetAttributeBuffers().Tangents);
                info.DrawContext->Resources().UVsSsbo = graph.AddExternal(m_Name.Name() + ".Uvs",
                    ctx.Geometry().GetAttributeBuffers().UVs);
                info.DrawContext->Resources().ObjectsSsbo = graph.AddExternal(m_Name.Name() + ".Objects",
                    ctx.Geometry().GetRenderObjectsBuffer());
            }
            
            auto& meshResources = ctx.MeshletContext().MeshContext().Resources();
            meshResources.HiZSampler = info.HiZContext->GetSampler();
            if constexpr(Stage != CullStage::Reocclusion)
                meshResources.HiZ = graph.Read(meshResources.HiZ, Compute | Sampled);
            meshResources.ObjectsSsbo = graph.Read(meshResources.ObjectsSsbo, Compute | Storage);

            auto& meshletResources = ctx.MeshletContext().Resources();
            meshletResources.VisibilitySsbo = graph.Read(meshletResources.VisibilitySsbo, Compute | Storage);
            meshletResources.CompactCommandsSsbo = graph.Read(meshletResources.CompactCommandsSsbo, Compute | Storage);
            meshletResources.CompactCountSsbo = graph.Read(meshletResources.CompactCountSsbo, Compute | Storage);

            auto& cullResources = ctx.Resources();
            cullResources.SceneUbo = graph.Read(cullResources.SceneUbo, Compute | Uniform | Upload);
            cullResources.TriangleVisibilitySsbo =
                graph.Read(cullResources.TriangleVisibilitySsbo, Compute | Storage);
            cullResources.TriangleVisibilitySsbo =
                graph.Write(cullResources.TriangleVisibilitySsbo, Compute | Storage);
            cullResources.IndicesSsbo = graph.Read(cullResources.IndicesSsbo, Compute | Storage);

            // draw subpass data
            auto& drawResources = info.DrawContext->Resources();
            auto& graphGlobals = graph.GetGlobalResources();
            drawResources.CameraUbo = graph.Read(graphGlobals.MainCameraGPU, Vertex | Uniform);
            drawResources.PositionsSsbo = graph.Read(drawResources.PositionsSsbo, Vertex | Storage);
            drawResources.NormalsSsbo = graph.Read(drawResources.NormalsSsbo, Vertex | Storage);
            drawResources.TangentsSsbo = graph.Read(drawResources.TangentsSsbo, Vertex | Storage);
            drawResources.UVsSsbo = graph.Read(drawResources.UVsSsbo, Vertex | Storage);
            drawResources.ObjectsSsbo = graph.Read(drawResources.ObjectsSsbo, Vertex | Storage);
            drawResources.CommandsSsbo = graph.Read(meshletResources.CommandsSsbo, Vertex | Storage);

            if (enumHasAny(m_Features, DrawFeatures::IBL))
            {
                ASSERT(info.IBL.has_value(), "IBL data is not provided")
                drawResources.IBL = {
                    .Irradiance = graph.Read(info.IBL->Irradiance, Pixel | Sampled),
                    .PrefilterEnvironment = graph.Read(info.IBL->PrefilterEnvironment, Pixel | Sampled),
                    .BRDF = graph.Read(info.IBL->BRDF, Pixel | Sampled)};
            }
            if (enumHasAny(m_Features, DrawFeatures::SSAO))
            {
                ASSERT(info.SSAO.has_value(), "SSAO data is not provided")
                drawResources.SSAO->SSAOTexture = graph.Read(info.SSAO->SSAOTexture, Pixel | Sampled);
            }

            for (u32 i = 0; i < ctx.MAX_BATCHES; i++)
            {
                cullResources.TrianglesSsbo[i] = graph.Write(cullResources.TrianglesSsbo[i], Compute | Storage);
                cullResources.TrianglesSsbo[i] = graph.Read(cullResources.TrianglesSsbo[i], Vertex | Storage);
                
                cullResources.IndicesCulledSsbo[i] =
                    graph.Write(cullResources.IndicesCulledSsbo[i], Compute | Storage);
                cullResources.IndicesCulledSsbo[i] =
                    graph.Read(cullResources.IndicesCulledSsbo[i], Compute | Storage | Vertex | Storage | Index);
                
                cullResources.DispatchIndirect = graph.Read(info.Dispatch, Compute | Indirect);
                
                ctx.Resources().IndicesCulledCountSsbo[i] = graph.Write(
                    ctx.Resources().IndicesCulledCountSsbo[i], Compute | Storage);
                ctx.Resources().IndicesCulledCountSsbo[i] = graph.Read(
                    ctx.Resources().IndicesCulledCountSsbo[i], Compute | Storage);
                
                ctx.Resources().DrawIndirect[i] = graph.Write(ctx.Resources().DrawIndirect[i], Compute | Storage);
                ctx.Resources().DrawIndirect[i] = graph.Read(ctx.Resources().DrawIndirect[i], Indirect);
            }

            info.DrawContext->Resources().RenderTargets.clear();
            info.DrawContext->Resources().DepthTarget = {};
            for (u32 attachmentIndex = 0; attachmentIndex < info.ColorAttachments.size(); attachmentIndex++)
            {
                auto& attachment = info.ColorAttachments[attachmentIndex];
                Resource resource = attachment.Resource;
                info.DrawContext->Resources().RenderTargets.push_back(graph.RenderTarget(
                    resource,
                    attachment.Description.OnLoad, attachment.Description.OnStore,
                    attachment.Description.Clear.Color.F));
            }
            if (info.DepthAttachment.has_value())
            {
                auto& attachment = *info.DepthAttachment;
                Resource resource = attachment.Resource;
                info.DrawContext->Resources().DepthTarget = graph.DepthStencilTarget(
                    resource,
                    attachment.Description.OnLoad, attachment.Description.OnStore,
                    attachment.Description.Clear.DepthStencil.Depth,
                    attachment.Description.Clear.DepthStencil.Stencil);
            }
        
            if constexpr(Stage != CullStage::Reocclusion)
                passData.HiZ = meshResources.HiZ;
            else
                passData.HiZ = graph.Read(
                    graph.GetBlackboard().Get<HiZPass::PassData>().HiZOut, Compute | Sampled);
            passData.HiZSampler = meshResources.HiZSampler;
            passData.ObjectsSsbo = meshResources.ObjectsSsbo;
            passData.MeshletVisibilitySsbo = meshletResources.VisibilitySsbo;
            passData.CompactCommandsSsbo = meshletResources.CompactCommandsSsbo;
            passData.CompactCountSsbo = meshletResources.CompactCountSsbo;
            passData.TriangleCullResources = cullResources;
            passData.TriangleDrawResources = drawResources;
            passData.DrawIndirect = ctx.Resources().DrawIndirect;

            passData.CullPipelines = &m_CullPipelines;
            passData.PreparePipelines = &m_PreparePipelines;
            passData.DrawPipelines = &m_DrawPipelines;

            passData.DrawFeatures = m_Features;
            
            passData.SplitBarriers = &m_SplitBarriers;
            passData.SplitBarrierDependency = &m_SplitBarrierDependency;

            PassData passDataPublic = {};
            passDataPublic.RenderTargets = passData.TriangleDrawResources.RenderTargets;
            passDataPublic.DepthTarget = passData.TriangleDrawResources.DepthTarget;
            graph.GetBlackboard().Update(m_Name.Hash(), passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Triangle Cull Draw")

            using enum DrawFeatures;

            const Texture& hiz = resources.GetTexture(passData.HiZ);
            const Sampler& hizSampler = passData.HiZSampler;
            SceneUBO scene = {};
            scene.ViewProjectionMatrix = frameContext.MainCamera->GetViewProjection();
            scene.FrustumPlanes = frameContext.MainCamera->GetFrustumPlanes();
            scene.ProjectionData = frameContext.MainCamera->GetProjectionData();
            scene.HiZWidth = (f32)hiz.GetDescription().Width;
            scene.HiZHeight = (f32)hiz.GetDescription().Height;
            const Buffer& sceneUbo = resources.GetBuffer(passData.TriangleCullResources.SceneUbo, scene,
                *frameContext.ResourceUploader);
            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& meshletVisibilitySsbo = resources.GetBuffer(passData.MeshletVisibilitySsbo);
            const Buffer& compactCommandsSsbo = resources.GetBuffer(passData.CompactCommandsSsbo);
            const Buffer& compactCountSsbo = resources.GetBuffer(passData.CompactCountSsbo);
            const Buffer& triangleVisibilitySsbo = resources.GetBuffer(
                passData.TriangleCullResources.TriangleVisibilitySsbo);

            std::array<Buffer, TriangleCullContext::MAX_BATCHES> indicesCulledCountSsbo;
            std::array<Buffer, TriangleCullContext::MAX_BATCHES> drawIndirectSsbo;
            std::array<Buffer, TriangleCullContext::MAX_BATCHES> trianglesSsbo;
            std::array<Buffer, TriangleCullContext::MAX_BATCHES> indicesCulledSsbo;
            for (u32 i = 0; i < trianglesSsbo.size(); i++)
            {
                indicesCulledCountSsbo[i] = resources.GetBuffer(passData.TriangleCullResources.IndicesCulledCountSsbo[i]);
                drawIndirectSsbo[i] = resources.GetBuffer(passData.DrawIndirect[i]); 
                trianglesSsbo[i] = resources.GetBuffer(passData.TriangleCullResources.TrianglesSsbo[i]);
                indicesCulledSsbo[i] = resources.GetBuffer(passData.TriangleCullResources.IndicesCulledSsbo[i]);
            }

            const Buffer& cameraUbo = resources.GetBuffer(passData.TriangleDrawResources.CameraUbo); 
            const Buffer& positionsSsbo = resources.GetBuffer(passData.TriangleDrawResources.PositionsSsbo);
            const Buffer& normalsSsbo = resources.GetBuffer(passData.TriangleDrawResources.NormalsSsbo);
            const Buffer& tangentsSsbo = resources.GetBuffer(passData.TriangleDrawResources.TangentsSsbo);
            const Buffer& uvSsbo = resources.GetBuffer(passData.TriangleDrawResources.UVsSsbo);
            const Buffer& commandsSsbo = resources.GetBuffer(passData.TriangleDrawResources.CommandsSsbo);
            const Buffer& indicesSsbo = resources.GetBuffer(passData.TriangleCullResources.IndicesSsbo);
            const Buffer& dispatchSsboIndirect = resources.GetBuffer(passData.TriangleCullResources.DispatchIndirect);

            auto updateIterationCull = [&](u32 index)
            {
                auto& samplerDescriptors = passData.CullPipelines->at(index).SamplerDescriptors;
                auto& resourceDescriptors = passData.CullPipelines->at(index).ResourceDescriptors;

                samplerDescriptors.UpdateBinding("u_sampler", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
                resourceDescriptors.UpdateBinding("u_hiz", hiz.BindingInfo(hizSampler, ImageLayout::Readonly));
                resourceDescriptors.UpdateBinding("u_scene_data", sceneUbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_meshlet_visibility", meshletVisibilitySsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_commands", compactCommandsSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_count", compactCountSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_triangle_visibility", triangleVisibilitySsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_positions", positionsSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_indices", indicesSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_triangles", trianglesSsbo[index].BindingInfo());
                resourceDescriptors.UpdateBinding("u_culled_indices", indicesCulledSsbo[index].BindingInfo());
                resourceDescriptors.UpdateBinding("u_culled_count", indicesCulledCountSsbo[index].BindingInfo());
            };
            auto updateIterationPrepare = [&](u32 index)
            {
                auto& resourceDescriptors = passData.PreparePipelines->at(index).ResourceDescriptors;
                resourceDescriptors.UpdateBinding("u_index_count", indicesCulledCountSsbo[index].BindingInfo());
                resourceDescriptors.UpdateBinding("u_indirect_draw", drawIndirectSsbo[index].BindingInfo());
            };
            auto updateIterationDraw = [&](u32 index)
            {
                auto& resourceDescriptors = passData.DrawPipelines->at(index).ResourceDescriptors;

                resourceDescriptors.UpdateBinding("u_camera", cameraUbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
                resourceDescriptors.UpdateBinding("u_commands", commandsSsbo.BindingInfo());

                if (enumHasAny(passData.DrawFeatures, Triangles))
                    resourceDescriptors.UpdateBinding("u_triangles", trianglesSsbo[index].BindingInfo());
                if (enumHasAny(passData.DrawFeatures, Positions))
                    resourceDescriptors.UpdateBinding("u_positions", positionsSsbo.BindingInfo());
                if (enumHasAny(passData.DrawFeatures, Normals))
                    resourceDescriptors.UpdateBinding("u_normals", normalsSsbo.BindingInfo());
                if (enumHasAny(passData.DrawFeatures, Tangents))
                    resourceDescriptors.UpdateBinding("u_tangents", tangentsSsbo.BindingInfo());
                if (enumHasAny(passData.DrawFeatures, UV))
                    resourceDescriptors.UpdateBinding("u_uv", uvSsbo.BindingInfo());

                if (enumHasAny(passData.DrawFeatures, IBL))
                    RgUtils::updateIBLBindings(resourceDescriptors, resources, *passData.TriangleDrawResources.IBL);
                
                if (enumHasAny(passData.DrawFeatures, SSAO))
                    RgUtils::updateSSAOBindings(resourceDescriptors, resources, *passData.TriangleDrawResources.SSAO);
            };
            auto createRenderingInfo = [&](bool canClear)
            {
                RenderingInfo::Builder renderingInfoBuilder = RenderingInfo::Builder()
                    .SetResolution(info.Resolution);
                auto& colors = passData.TriangleDrawResources.RenderTargets;
                auto& depth = passData.TriangleDrawResources.DepthTarget;
                for (u32 attachmentIndex = 0; attachmentIndex < colors.size(); attachmentIndex++)
                {
                    auto description = info.ColorAttachments[attachmentIndex].Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    const Texture& colorTexture = resources.GetTexture(colors[attachmentIndex]);
                    renderingInfoBuilder.AddAttachment(RenderingAttachment::Builder(description)
                        .FromImage(colorTexture, ImageLayout::Attachment)
                        .Build(frameContext.DeletionQueue));
                }
                if (depth.has_value())
                {
                    auto description = info.DepthAttachment->Description;
                    if (!canClear)
                        description.OnLoad = AttachmentLoad::Load;

                    const Texture& depthTexture = resources.GetTexture(*depth);
                    renderingInfoBuilder.AddAttachment(RenderingAttachment::Builder(description)
                        .FromImage(depthTexture, ImageLayout::DepthAttachment)
                        .Build(frameContext.DeletionQueue));
                }

                RenderingInfo renderingInfo = renderingInfoBuilder.Build(frameContext.DeletionQueue);

                return renderingInfo;
            };
            auto waitOnBarrier = [&](u32 batchIndex, u32 index)
            {
                if (index < TriangleCullContext::MAX_BATCHES)
                    return;

                passData.SplitBarriers->at(batchIndex).Wait(frameContext.Cmd, *passData.SplitBarrierDependency);
                passData.SplitBarriers->at(batchIndex).Reset(frameContext.Cmd, *passData.SplitBarrierDependency);
            };
            auto signalBarrier = [&](u32 batchIndex)
            {
                passData.SplitBarriers->at(batchIndex).Signal(frameContext.Cmd, *passData.SplitBarrierDependency);
            };
            
            // update descriptors
            for (u32 i = 0; i < TriangleCullContext::MAX_BATCHES; i++)
            {
                updateIterationCull(i);
                updateIterationPrepare(i);
                updateIterationDraw(i);
            }

            // bind the immutable samplers and textures once
            if (enumHasAny(passData.DrawFeatures, Textures))
            {
                auto& pipeline = passData.DrawPipelines->at(0).Pipeline;
                pipeline.BindGraphics(frameContext.Cmd);
                passData.DrawPipelines->at(0).ImmutableSamplerDescriptors.BindGraphicsImmutableSamplers(
                    frameContext.Cmd, pipeline.GetLayout());
                passData.DrawPipelines->at(0).MaterialDescriptors.BindGraphics(frameContext.Cmd,
                            resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            }

            // if there are no batches, we clear the screen (if needed)
            if (info.CullContext->GetIterationCount() == 0)
            {
                auto& cmd = frameContext.Cmd;
                RenderCommand::SetViewport(cmd, info.Resolution);
                RenderCommand::SetScissors(cmd, {0, 0}, info.Resolution);
                RenderCommand::BeginRendering(cmd, createRenderingInfo(true));
                RenderCommand::EndRendering(cmd);

                return;
            }
            
            for (u32 i = 0; i < info.CullContext->GetIterationCount(); i++)
            {
                u32 batchIndex = i % TriangleCullContext::MAX_BATCHES;

                waitOnBarrier(batchIndex, i);
                
                // cull
                {
                    auto& pipeline = passData.CullPipelines->at(batchIndex).Pipeline;
                    auto& samplerDescriptors = passData.CullPipelines->at(batchIndex).SamplerDescriptors;
                    auto& resourceDescriptors = passData.CullPipelines->at(batchIndex).ResourceDescriptors;

                    struct PushConstants
                    {
                        u32 ScreenWidth;
                        u32 ScreenHeight;
                        u32 CommandOffset;
                        u32 MaxCommandIndex;
                    };
                    PushConstants pushConstants = {
                        .ScreenWidth = frameContext.Resolution.x,
                        .ScreenHeight = frameContext.Resolution.y,
                        .CommandOffset = i * TriangleCullContext::GetCommandCount(),
                        .MaxCommandIndex = TriangleCullContext::GetCommandCount()};
                    auto& cmd = frameContext.Cmd;
                    pipeline.BindCompute(cmd);
                    RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
                    samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                        pipeline.GetLayout());
                    resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                        pipeline.GetLayout());
                    RenderCommand::DispatchIndirect(cmd, dispatchSsboIndirect, i * sizeof(IndirectDispatchCommand));

                    MemoryDependencyInfo dependency = {
                        .SourceStage = PipelineStage::ComputeShader,
                        .DestinationStage = PipelineStage::ComputeShader,
                        .SourceAccess = PipelineAccess::WriteShader,
                        .DestinationAccess = PipelineAccess::ReadShader};
                    RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                        .MemoryDependency(dependency)
                        .Build(frameContext.DeletionQueue));
                }
                {
                    auto& pipeline = passData.PreparePipelines->at(batchIndex).Pipeline;
                    auto& resourceDescriptors = passData.PreparePipelines->at(batchIndex).ResourceDescriptors;
                    
                    auto& cmd = frameContext.Cmd;
                    pipeline.BindCompute(cmd);
                    resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(),
                        pipeline.GetLayout());
                    RenderCommand::Dispatch(cmd, {1, 1, 1});

                    MemoryDependencyInfo dependency = {
                        .SourceStage = PipelineStage::ComputeShader,
                        .DestinationStage = PipelineStage::Indirect,
                        .SourceAccess = PipelineAccess::WriteShader,
                        .DestinationAccess = PipelineAccess::ReadIndirect};
                    RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                        .MemoryDependency(dependency)
                        .Build(frameContext.DeletionQueue));
                }
                // render
                {
                    auto& pipeline = passData.DrawPipelines->at(batchIndex).Pipeline;
                    auto& resourceDescriptors = passData.DrawPipelines->at(batchIndex).ResourceDescriptors;

                    auto& cmd = frameContext.Cmd;
                    RenderCommand::SetViewport(cmd, info.Resolution);
                    RenderCommand::SetScissors(cmd, {0, 0}, info.Resolution);
                    RenderCommand::BeginRendering(cmd, createRenderingInfo(i == 0));

                    RenderCommand::BindIndexU32Buffer(cmd, indicesCulledSsbo[batchIndex], 0);
                    pipeline.BindGraphics(cmd);
                    resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(),
                        pipeline.GetLayout());

                    RenderCommand::DrawIndexedIndirect(cmd, drawIndirectSsbo[batchIndex], 0, 1);                    
                    
                    RenderCommand::EndRendering(cmd);
                }

                signalBarrier(batchIndex);
            }
        });
}
