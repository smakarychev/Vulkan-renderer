#include "rendererpch.h"

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
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = depthIn,
                    .Format = Format::RGBA16_FLOAT});

            passData.DepthIn = graph.ReadImage(depthIn, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            GPU_PROFILE_FRAME("Visualize Depth")

            const Shader& shader = graph.GetShader();
            DepthVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetDepth(graph.GetImageBinding(passData.DepthIn));

            struct PushConstants
            {
                f32 Near{1.0f};
                f32 Far{100.0f};
                GpuBool IsOrthographic{false};
            };
            PushConstants pushConstants = {
                .Near = near,
                .Far = far,
                .IsOrthographic = isOrthographic};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Draw({.VertexCount = 3});
        });
}
