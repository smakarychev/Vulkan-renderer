#include "VisibilityPass.h"

#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "RenderGraph/Passes/Generated/VisibilityBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Draw::Visibility::addToGraph(StringId name, RG::Graph& renderGraph,
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
                    .CullTriangles = true});
                multiview.MultiviewData.SetPrimaryView(0);
                multiview.MultiviewData.Finalize();
            }
            
            Resource visibility = renderGraph.CreateResource("VisibilityBuffer"_hsv,
                GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format = Format::R32_UINT});

            Resource depth = renderGraph.CreateResource("Depth"_hsv,
                GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format = Format::D32_FLOAT});

            auto& multiview = graph.GetBlackboardValue<Multiview>();
            multiview.MultiviewData.UpdateView(0, {
                .Resolution = info.Resolution,
                .Camera = info.Camera,
                .DrawInfo = {
                    .DrawSetup = [&](Graph&) {},
                    .DrawBind = [=](RenderCommandList& cmdList, const Resources& resources,
                        const GeometryDrawExecutionInfo& executionInfo) -> const Shader&
                    {
                        const Shader& shader = ShaderCache::Register(
                            StringId("{}.Draw.{}.{}",
                                name, executionInfo.Triangles.IsValid(), executionInfo.ExecutionId),
                            "visibility.shader", 
                            ShaderSpecializations{
                                ShaderSpecialization{"COMPOUND_INDEX"_hsv, executionInfo.Triangles.IsValid()}});
                        VisibilityShaderBindGroup bindGroup(shader);
                        
                        bindGroup.SetCamera({.Buffer = resources.GetBuffer(executionInfo.Camera)});
                        bindGroup.SetObjects({.Buffer = resources.GetBuffer(executionInfo.Objects)});
                        bindGroup.SetCommands({.Buffer = resources.GetBuffer(executionInfo.Commands)});
                        RgUtils::updateDrawAttributeBindings(bindGroup, resources, executionInfo.DrawAttributes);
                        if (executionInfo.Triangles.IsValid())
                            bindGroup.SetTriangles({.Buffer = resources.GetBuffer(executionInfo.Triangles)});

                        bindGroup.Bind(cmdList, resources.GetGraph()->GetArenaAllocators());
                        
                        return shader;
                    },
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

            auto& meta = Meta::CullMultiview::addToGraph(name.Concatenate(".Meta"), renderGraph,
                multiview.MultiviewData);
            auto& metaOutput = renderGraph.GetBlackboard().Get<Meta::CullMultiview::PassData>(meta);
            passData.ColorOut = metaOutput.DrawAttachmentResources[0].Colors[0];
            passData.DepthOut = *metaOutput.DrawAttachmentResources[0].Depth;
            passData.HiZOut = metaOutput.HiZOut[0];
            passData.HiZMaxOut = metaOutput.HiZMaxOut;
            passData.MinMaxDepth = metaOutput.MinMaxDepth;
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}
