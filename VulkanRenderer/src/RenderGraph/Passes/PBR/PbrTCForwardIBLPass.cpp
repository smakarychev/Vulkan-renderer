#include "PbrTCForwardIBLPass.h"

#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMultiviewData.h"
#include "RenderGraph/Passes/Generated/PbrForwardBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Pbr::ForwardTcIbl::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const PbrForwardIBLPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    //todo: this should be shared between all multiview passes obv.
    struct Multiview
    {
        CullMultiviewData MultiviewData{};
    };
    IBLData iblData{};
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Forward.IBL.Setup")

            if (!graph.TryGetBlackboardValue<Multiview>())
            {
                Multiview multiview = {};
                multiview.MultiviewData.AddView({
                    .Geometry = info.Geometry,
                    .CullTriangles = true});

                multiview.MultiviewData.Finalize();
            }

            auto& multiview = graph.GetBlackboardValue<Multiview>();
            multiview.MultiviewData.UpdateView(0, {
                .Resolution = info.Resolution,
                .Camera = info.Camera,
                .DrawInfo = {
                    .DrawSetup = [&](Graph& setupGraph)
                    {
                        iblData = RgUtils::readIBLData(info.IBL, setupGraph, Pixel);
                    },
                    .DrawBind = [=](RenderCommandList& cmdList, const Resources& resources,
                        const GeometryDrawExecutionInfo& executionInfo) -> const Shader&
                    {
                        const Shader& shader = ShaderCache::Register(
                            std::format("{}.Draw.{}.{}",
                                name, executionInfo.Triangles.IsValid(), executionInfo.ExecutionId),
                            "pbr-forward.shader",
                            ShaderOverrides{
                                ShaderOverride{"MAX_REFLECTION_LOD"_hsv,
                                    (f32)ImageUtils::mipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION})},
                                ShaderOverride{"COMPOUND_INDEX"_hsv, executionInfo.Triangles.IsValid()}});
                        PbrForwardShaderBindGroup bindGroup(shader);
                        
                        bindGroup.SetCamera({.Buffer = resources.GetBuffer(executionInfo.Camera)});
                        bindGroup.SetObjects({.Buffer = resources.GetBuffer(executionInfo.Objects)});
                        bindGroup.SetCommands({.Buffer = resources.GetBuffer(executionInfo.Commands)});
                        RgUtils::updateDrawAttributeBindings(bindGroup, resources, executionInfo.DrawAttributes);
                        RgUtils::updateIBLBindings(bindGroup, resources, iblData);
                        
                        bindGroup.Bind(cmdList, resources.GetGraph()->GetArenaAllocators());
                        
                        return shader;
                    },
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
                                .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}}}}}});

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
