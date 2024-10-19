#include "SimpleAtmospherePass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
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

            graph.SetShader("../assets/shaders/atmosphere-simple.shader");

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
            auto& pipeline = shader.Pipeline();
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_transmittance_lut",
              resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                  ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.Camera).BindingInfo());

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), (f32)frameContext.FrameNumberTick);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
}
