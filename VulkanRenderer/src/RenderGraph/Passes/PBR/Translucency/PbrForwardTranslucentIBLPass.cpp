#include "PbrForwardTranslucentIBLPass.h"

#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/MeshCullMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/MeshletCullTranslucentMultiviewPass.h"
#include "RenderGraph/Passes/General/DrawIndirectPass.h"
#include "RenderGraph/Passes/Generated/PbrForwardTranslucentBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Pbr::ForwardTranslucentIbl::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const PbrForwardTranslucentIBLPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    //todo: this should be shared between all multiview passes obv.
    struct CullResources
    {
        CullMultiviewData MultiviewData{};
        CullMultiviewResources MultiviewResource{};
    };
    IBLData iblData = {};
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Forward.Translucent.IBL.Setup")
            
            if (!graph.TryGetBlackboardValue<CullResources>())
            {
                CullResources cullResources = {};
                cullResources.MultiviewData.AddView({
                    .Geometry = info.Geometry,
                    .HiZContext = info.HiZContext,
                    .CullTriangles = false});

                cullResources.MultiviewData.Finalize();
            }

            auto& cullResources = graph.GetBlackboardValue<CullResources>();
            cullResources.MultiviewResource = RgUtils::createCullMultiview(cullResources.MultiviewData, graph,
                std::string{name});

            Multiview::MeshCull::addToGraph(std::format("{}.Mesh.Cull", name),
                renderGraph, {.MultiviewResource = &cullResources.MultiviewResource}, CullStage::Single);
            auto& meshletCull = Multiview::MeshletCullTranslucent::addToGraph(std::format("{}.Meshlet.Cull", name),
                renderGraph, {.MultiviewResource = &cullResources.MultiviewResource});
            auto& meshletCullOutput = renderGraph.GetBlackboard().Get<Multiview::MeshletCullTranslucent::PassData>(
                meshletCull);

            auto& draw = Draw::Indirect::addToGraph(std::format("{}.Draw", name),
                renderGraph, {
                    .Geometry = info.Geometry,
                    .Commands = meshletCullOutput.MultiviewResource->CompactCommands[0],
                    .Resolution = info.Resolution,
                    .Camera = info.Camera,
                    .DrawInfo = {
                        .DrawSetup = [&](Graph& setupGraph)
                        {
                            iblData = RgUtils::readIBLData(info.IBL, setupGraph, Pixel);
                        },
                        .DrawBind = [=](CommandBuffer cmd, const Resources& resources,
                            const GeometryDrawExecutionInfo& executionInfo) -> const Shader&
                        {
                            const Shader& shader = ShaderCache::Register(std::format("{}.Draw", name),
                                "pbr-forward-translucent.shader", {});
                            PbrForwardTranslucentShaderBindGroup bindGroup(shader);
                            
                            bindGroup.SetCamera({.Buffer = resources.GetBuffer(executionInfo.Camera)});
                            bindGroup.SetObjects({.Buffer = resources.GetBuffer(executionInfo.Objects)});
                            bindGroup.SetCommands({.Buffer = resources.GetBuffer(executionInfo.Commands)});
                            RgUtils::updateDrawAttributeBindings(bindGroup, resources, executionInfo.DrawAttributes);
                            RgUtils::updateIBLBindings(bindGroup, resources, iblData);
                            
                            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                            
                            return shader;
                        },
                        .Attachments = {
                            .Colors = {DrawAttachment{
                                .Resource = info.ColorIn,
                                .Description = {
                                    .OnLoad = info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                                    .ClearColor = {.F = {0.1f, 0.1f, 0.1f, 1.0f}}}}},
                            .Depth = DepthStencilAttachment{
                                .Resource = info.DepthIn,
                                .Description = {
                                    .OnLoad = AttachmentLoad::Load}}}}});
            auto& drawOutput = renderGraph.GetBlackboard().Get<Draw::Indirect::PassData>(draw);
            passData.ColorOut = drawOutput.DrawAttachmentResources.Colors[0];
            passData.DepthOut = *drawOutput.DrawAttachmentResources.Depth;
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}
