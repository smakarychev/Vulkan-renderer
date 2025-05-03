#include "SimpleAtmospherePass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereSimpleBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::AtmosphereSimple::addToGraph(StringId name, RG::Graph& renderGraph,
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
            passData.ColorOut = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Simple")
            GPU_PROFILE_FRAME("Atmosphere.Simple")

            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereSimpleShaderBindGroup bindGroup(shader);
            bindGroup.SetTransmittanceLut({.Image = resources.GetTexture(passData.TransmittanceLut)},
                ImageLayout::Readonly);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {(f32)frameContext.FrameNumberTick}});
            cmd.Draw({.VertexCount = 3});
        });
}
