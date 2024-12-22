#include "PbrForwardTranslucentIBLPass.h"

#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/MeshCullMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/MeshletCullTranslucentMultiviewPass.h"
#include "RenderGraph/Passes/General/DrawIndirectPass.h"
#include "Rendering/ShaderCache.h"

RG::Pass& Passes::Pbr::ForwardTranslucentIbl::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const PbrForwardTranslucentIBLPassExecutionInfo& info)
{
    using namespace RG;

    //todo: this should be shared between all multiview passes obv.
    struct CullResources
    {
        CullMultiviewData MultiviewData{};
        CullMultiviewResources MultiviewResource{};
    };
    
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
                    .DrawShader = &ShaderCache::Register(std::format("{}.Draw", name),
                        "../assets/shaders/pbr-forward-translucent.shader", {}),
                    .DrawTrianglesShader = nullptr,
                    .CullTriangles = false});

                cullResources.MultiviewData.Finalize();
            }

            auto& resources = graph.GetBlackboardValue<CullResources>();
            resources.MultiviewResource = RgUtils::createCullMultiview(resources.MultiviewData, graph,
                std::string{name});

            Multiview::MeshCull::addToGraph(std::format("{}.Mesh.Cull", name),
                renderGraph, {.MultiviewResource = &resources.MultiviewResource}, CullStage::Single);
            auto& meshletCull = Multiview::MeshletCullTranslucent::addToGraph(std::format("{}.Meshlet.Cull", name),
                renderGraph, {.MultiviewResource = &resources.MultiviewResource});
            auto& meshletCullOutput = renderGraph.GetBlackboard().Get<Multiview::MeshletCullTranslucent::PassData>(
                meshletCull);

            auto& draw = Draw::Indirect::addToGraph(std::format("{}.Draw", name),
                renderGraph, {
                    .Geometry = info.Geometry,
                    .Commands = meshletCullOutput.MultiviewResource->CompactCommands[0],
                    .Resolution = info.Resolution,
                    .Camera = info.Camera,
                    .DrawInfo = {
                        .Attachments = {
                        .Colors = {DrawAttachment{
                            .Resource = info.ColorIn,
                            .Description = {
                                .OnLoad = info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                                .ClearColor = {.F = {0.1f, 0.1f, 0.1f, 1.0f}}}}},
                        .Depth = DepthStencilAttachment{
                            .Resource = info.DepthIn,
                            .Description = {
                                .OnLoad = AttachmentLoad::Load}}},
                        .SceneLights = info.SceneLights,
                        .IBL = info.IBL},
                    .Shader = resources.MultiviewData.View(0).Static.DrawShader});
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
