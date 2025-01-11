#include "VisibilityPass.h"

#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "Rendering/ShaderCache.h"

RG::Pass& Passes::Draw::Visibility::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const VisibilityPassExecutionInfo& info)
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
            CPU_PROFILE_FRAME("Visibility.Setup")
            
            if (!graph.TryGetBlackboardValue<Multiview>())
            {
                Multiview& multiview = graph.GetOrCreateBlackboardValue<Multiview>();
                multiview.MultiviewData.AddView({
                    .Geometry = info.Geometry,
                    .DrawShader = &ShaderCache::Register(std::format("{}.Draw", name),
                        "../assets/shaders/visibility.shader", {}),
                    .DrawTrianglesShader = &ShaderCache::Register(std::format("{}.Draw.Triangles", name),
                        "../assets/shaders/visibility.shader", 
                        ShaderOverrides{
                            ShaderOverride{{"COMPOUND_INDEX"}, true}}),
                    .CullTriangles = true});
                multiview.MultiviewData.SetPrimaryView(0);
                multiview.MultiviewData.Finalize();
            }
            
            Resource visibility = renderGraph.CreateResource("VisibilityBuffer.VisibilityBuffer",
                GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format = Format::R32_UINT});

            Resource depth = renderGraph.CreateResource("VisibilityBuffer.Depth",
                GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format = Format::D32_FLOAT});

            auto& multiview = graph.GetBlackboardValue<Multiview>();
            multiview.MultiviewData.UpdateView(0, {
                .Resolution = info.Resolution,
                .Camera = info.Camera,
                .DrawInfo = {
                    .Attachments = {
                        .Colors = {DrawAttachment{
                            .Resource = visibility,
                            .Description = {
                                .OnLoad = AttachmentLoad::Clear,
                                .ClearColor = {.U = glm::uvec4{std::numeric_limits<u32>::max(), 0, 0, 0}}}}},
                        .Depth = DepthStencilAttachment{
                            .Resource = depth,
                            .Description = {
                                .OnLoad = AttachmentLoad::Clear,
                                .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}}}}}});

            auto& meta = Meta::CullMultiview::addToGraph(std::format("{}.Meta", name), renderGraph,
                multiview.MultiviewData);
            auto& metaOutput = renderGraph.GetBlackboard().Get<Meta::CullMultiview::PassData>(meta);
            passData.ColorOut = metaOutput.DrawAttachmentResources[0].Colors[0];
            passData.DepthOut = *metaOutput.DrawAttachmentResources[0].Depth;
            passData.HiZOut = metaOutput.HiZOut[0];
            passData.HiZMaxOut = metaOutput.HiZMaxOut;
            passData.MinMaxDepth = metaOutput.MinMaxDepth;
            passData.PreviousMinMaxDepth = graph.AddExternal(std::format("{}.PreviousMinMaxDepth", name),
                multiview.MultiviewData.View(0).Static.HiZContext->GetPreviousMinMaxDepthBuffer());
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}
