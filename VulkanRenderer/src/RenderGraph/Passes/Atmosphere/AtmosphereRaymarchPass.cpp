#include "AtmosphereRaymarchPass.h"

#include "CameraGPU.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Atmosphere::Raymarch::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource atmosphereSettings, const Camera& camera, const SceneLight& light,
    RG::Resource skyViewLut, RG::Resource transmittanceLut, RG::Resource aerialPerspectiveLut,
    RG::Resource colorIn, const ImageSubresourceDescription& colorSubresource,
    RG::Resource depthIn, bool useSunLuminance)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
    [&](Graph& graph, PassData& passData)
    {
        CPU_PROFILE_FRAME("Atmosphere.Raymarch.Setup")

        graph.SetShader("atmosphere-raymarch.shader");

        passData.DirectionalLight = graph.AddExternal(std::format("{}.DirectionalLight", name),
            light.GetBuffers().DirectionalLight);
        auto& globalResources = graph.GetGlobalResources();
        passData.ColorOut = RgUtils::ensureResource(colorIn, graph, std::format("{}.ColorOut", name),
            GraphTextureDescription{
                .Width = globalResources.Resolution.x,
                .Height = globalResources.Resolution.y,
                .Format = Format::RGBA16_FLOAT});

        passData.Camera = graph.CreateResource(std::format("{}.Camera", name), GraphBufferDescription{
            .SizeBytes = sizeof(CameraGPU)});
        graph.Upload(passData.Camera, CameraGPU::FromCamera(camera, globalResources.Resolution));

        if (depthIn.IsValid())
            passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);

        passData.SkyViewLut = graph.Read(skyViewLut, Pixel | Sampled);
        if (transmittanceLut.IsValid())
            passData.TransmittanceLut = graph.Read(transmittanceLut, Pixel | Sampled);
        if (aerialPerspectiveLut.IsValid())
            passData.AerialPerspectiveLut = graph.Read(aerialPerspectiveLut, Pixel | Sampled);
        passData.AtmosphereSettings = graph.Read(atmosphereSettings, Pixel | Uniform);
        passData.DirectionalLight = graph.Read(passData.DirectionalLight, Pixel | Uniform);
        passData.Camera = graph.Read(passData.Camera, Pixel | Uniform);
        passData.ColorOut = graph.RenderTarget(passData.ColorOut, colorSubresource,
            AttachmentLoad::Load, AttachmentStore::Store, {});

        graph.UpdateBlackboard(passData);
    },
    [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
    {
        CPU_PROFILE_FRAME("Atmosphere.Raymarch")
        GPU_PROFILE_FRAME("Atmosphere.Raymarch")

        const Shader& shader = resources.GetGraph()->GetShader();
        auto pipeline = shader.Pipeline(); 
        auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
        auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

        if (passData.DepthIn.IsValid())
            resourceDescriptors.UpdateBinding("u_depth", resources.GetTexture(passData.DepthIn).BindingInfo(
                ImageFilter::Linear, ImageLayout::DepthReadonly));
        
        resourceDescriptors.UpdateBinding("u_atmosphere_settings",
            resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
        resourceDescriptors.UpdateBinding("u_directional_light",
            resources.GetBuffer(passData.DirectionalLight).BindingInfo());
        resourceDescriptors.UpdateBinding("u_camera",
            resources.GetBuffer(passData.Camera).BindingInfo());
        resourceDescriptors.UpdateBinding("u_sky_view_lut",
            resources.GetTexture(passData.SkyViewLut).BindingInfo(
               ImageFilter::Linear, ImageLayout::Readonly));
        if (passData.TransmittanceLut.IsValid())
            resourceDescriptors.UpdateBinding("u_transmittance_lut",
                resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));
        if (passData.AerialPerspectiveLut.IsValid())
            resourceDescriptors.UpdateBinding("u_aerial_perspective_lut",
                resources.GetTexture(passData.AerialPerspectiveLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));

        struct PushConstant
        {
            bool UseDepthBuffer;
            bool UseSunLuminance;
        };
        PushConstant pushConstant = {
            .UseDepthBuffer = passData.DepthIn.IsValid(),
            .UseSunLuminance = useSunLuminance};
        
        auto& cmd = frameContext.Cmd;
        samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
        RenderCommand::BindGraphics(cmd, pipeline);
        RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);
        resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
        RenderCommand::Draw(cmd, 3);
    });
}
