#include "SimpleAtmospherePass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereSimpleBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::AtmosphereSimple::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Simple.Setup")

            graph.SetShader("atmosphere-simple.shader");

            auto& globalResources = graph.GetGlobalResources();
            passData.ColorOut = graph.CreateResource(std::string{name} + ".Color",
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
            bindGroup.SetTransmittanceLut(resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                  ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::PushConstants(cmd, shader.GetLayout(), (f32)frameContext.FrameNumberTick);
            RenderCommand::Draw(cmd, 3);
        });
}
