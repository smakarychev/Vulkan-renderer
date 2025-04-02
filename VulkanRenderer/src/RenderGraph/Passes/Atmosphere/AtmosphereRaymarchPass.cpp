#include "AtmosphereRaymarchPass.h"

#include "CameraGPU.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/AtmosphereRaymarchBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Atmosphere::Raymarch::addToGraph(StringId name, RG::Graph& renderGraph,
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

        passData.DirectionalLight = graph.AddExternal("DirectionalLight"_hsv,
            light.GetBuffers().DirectionalLight);
        auto& globalResources = graph.GetGlobalResources();
        passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "ColorOut"_hsv,
            GraphTextureDescription{
                .Width = globalResources.Resolution.x,
                .Height = globalResources.Resolution.y,
                .Format = Format::RGBA16_FLOAT});

        passData.Camera = graph.CreateResource("Camera"_hsv, GraphBufferDescription{
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
        AtmosphereRaymarchShaderBindGroup bindGroup(shader);
        if (passData.DepthIn.IsValid())
            bindGroup.SetDepth({.Image = resources.GetTexture(passData.DepthIn)}, ImageLayout::DepthReadonly);

        bindGroup.SetAtmosphereSettings({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
        bindGroup.SetDirectionalLight({.Buffer = resources.GetBuffer(passData.DirectionalLight)});
        bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
        bindGroup.SetSkyViewLut({.Image = resources.GetTexture(passData.SkyViewLut)}, ImageLayout::Readonly);
        if (passData.TransmittanceLut.IsValid())
            bindGroup.SetTransmittanceLut({.Image = resources.GetTexture(passData.TransmittanceLut)},
                ImageLayout::Readonly);
        if (passData.AerialPerspectiveLut.IsValid())
            bindGroup.SetAerialPerspectiveLut({.Image = resources.GetTexture(passData.AerialPerspectiveLut)},
                ImageLayout::Readonly);

        struct PushConstant
        {
            bool UseDepthBuffer;
            bool UseSunLuminance;
        };
        PushConstant pushConstant = {
            .UseDepthBuffer = passData.DepthIn.IsValid(),
            .UseSunLuminance = useSunLuminance};
        
        auto& cmd = frameContext.CommandList;
        bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
        cmd.PushConstants({
            .PipelineLayout = shader.GetLayout(), 
            .Data = {pushConstant}});
        cmd.Draw({.VertexCount = 3});
    });
}
