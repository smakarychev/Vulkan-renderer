#include "VisualizeDepthPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/DepthVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::VisualizeDepth::PassData& Passes::VisualizeDepth::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource depthIn, RG::Resource colorIn, f32 near, f32 far, bool isOrthographic)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            graph.SetShader("depth-visualize"_hsv);
            
            auto& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "Color"_hsv,
                GraphTextureDescription{
                    .Width = depthDescription.Width,
                    .Height = depthDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Visualize Depth")

            auto&& [depthTexture, depthDescription] = resources.GetTextureWithDescription(passData.DepthIn);

            const Shader& shader = resources.GetGraph()->GetShader();
            DepthVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetDepth({.Image = depthTexture}, depthDescription.Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly :
                ImageLayout::DepthStencilReadonly);

            struct PushConstants
            {
                f32 Near{1.0f};
                f32 Far{100.0f};
                bool IsOrthographic{false};
            };
            PushConstants pushConstants = {
                .Near = near,
                .Far = far,
                .IsOrthographic = isOrthographic};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Draw({.VertexCount = 3});
        }).Data;
}
