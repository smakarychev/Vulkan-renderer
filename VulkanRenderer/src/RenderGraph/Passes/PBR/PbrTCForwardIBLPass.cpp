#include "PbrTCForwardIBLPass.h"

#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMultiviewData.h"
#include "Rendering/ShaderCache.h"

RG::Pass& Passes::Pbr::ForwardTcIbl::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const PbrForwardIBLPassExecutionInfo& info)
{
    using namespace RG;

    //todo: this should be shared between all multiview passes obv.
    struct Multiview
    {
        CullMultiviewData MultiviewData{};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Forward.IBL.Setup")

            if (!graph.TryGetBlackboardValue<Multiview>())
            {
                Multiview multiview = {};
                multiview.MultiviewData.AddView({
                    .Geometry = info.Geometry,
                    .DrawShader = &ShaderCache::Register(std::format("{}.Draw", name),
                        "../assets/shaders/pbr-forward.shader",
                        ShaderOverrides{}
                            .Add({"MAX_REFLECTION_LOD"},
                                (f32)Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION}))),
                    .DrawTrianglesShader = &ShaderCache::Register(std::format("{}.Draw.Triangles", name),
                        "../assets/shaders/pbr-forward.shader", 
                        ShaderOverrides{}
                            .Add({"MAX_REFLECTION_LOD"},
                                (f32)Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION}))
                            .Add({"COMPOUND_INDEX"}, true)),
                    .CullTriangles = true});

                multiview.MultiviewData.Finalize();
            }

            auto& multiview = graph.GetBlackboardValue<Multiview>();
            multiview.MultiviewData.UpdateView(0, {
                .Resolution = info.Resolution,
                .Camera = info.Camera,
                .DrawInfo = {
                    .Attachments = {
                        .Colors = {DrawAttachment{
                            .Resource = info.ColorIn,
                            .Description = {
                                .OnLoad = info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                                .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}}}},
                        .Depth = DepthStencilAttachment{
                            .Resource = info.DepthIn,
                            .Description = {
                                .OnLoad = info.DepthIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                                .ClearDepth = 0.0f,
                                .ClearStencil = 0}}},
                    .SceneLights = info.SceneLights,
                    .IBL = info.IBL}});

            auto& meta = Meta::CullMultiview::addToGraph("Visibility", renderGraph, multiview.MultiviewData);
            auto& metaOutput = renderGraph.GetBlackboard().Get<Meta::CullMultiview::PassData>(meta);
            passData.ColorOut = metaOutput.DrawAttachmentResources[0].Colors[0];
            passData.DepthOut = *metaOutput.DrawAttachmentResources[0].Depth;
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}
