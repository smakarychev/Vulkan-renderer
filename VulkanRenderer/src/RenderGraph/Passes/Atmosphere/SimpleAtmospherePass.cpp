#include "SimpleAtmospherePass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/AtmosphereSimpleBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::AtmosphereSimple::PassData& Passes::AtmosphereSimple::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Simple.Setup")

            graph.SetShader("atmosphere-simple"_hsv);

            auto& globalResources = graph.GetGlobalResources();
            passData.ColorOut = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Width = (f32)globalResources.Resolution.x,
                    .Height = (f32)globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.Camera = graph.ReadBuffer(globalResources.PrimaryCameraGPU, Pixel | Uniform);
            passData.TransmittanceLut = graph.ReadImage(transmittanceLut, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Simple")
            GPU_PROFILE_FRAME("Atmosphere.Simple")

            const Shader& shader = graph.GetShader();
            AtmosphereSimpleShaderBindGroup bindGroup(shader);
            bindGroup.SetTransmittanceLut(graph.GetImageBinding(passData.TransmittanceLut));
            bindGroup.SetCamera(graph.GetBufferBinding(passData.Camera));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {(f32)frameContext.FrameNumberTick}});
            cmd.Draw({.VertexCount = 3});
        });
}
