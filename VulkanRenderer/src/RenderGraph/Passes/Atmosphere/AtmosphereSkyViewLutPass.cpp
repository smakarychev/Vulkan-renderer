#include "AtmosphereSkyViewLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereSkyViewLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneLight.h"

Passes::Atmosphere::SkyView::PassData& Passes::Atmosphere::SkyView::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut, RG::Resource multiscatteringLut,
    RG::Resource atmosphereSettings, const SceneLight& light)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView.Setup")

            graph.SetShader("atmosphere-sky-view-lut"_hsv);

            auto& globalResources = graph.GetGlobalResources();
            
            passData.Lut = graph.CreateResource("Lut"_hsv, GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Width"_hsv),
                .Height = (u32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Height"_hsv),
                .Format = Format::RGBA16_FLOAT});
            passData.DirectionalLight = graph.AddExternal("DirectionalLight"_hsv,
                light.GetBuffers().DirectionalLights);
            
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Compute | Sampled);
            passData.MultiscatteringLut = graph.Read(multiscatteringLut, Compute | Sampled);
            passData.DirectionalLight = graph.Read(passData.DirectionalLight, Compute | Uniform);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Compute | Uniform);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView")
            GPU_PROFILE_FRAME("Atmosphere.SkyView")

            auto&& [lutTexture, lutDescription] = resources.GetTextureWithDescription(passData.Lut);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereSkyViewLutShaderBindGroup bindGroup(shader);
            bindGroup.SetAtmosphereSettings({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetDirectionalLight({.Buffer = resources.GetBuffer(passData.DirectionalLight)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetTransmittanceLut({.Image = resources.GetTexture(passData.TransmittanceLut)}, 
                ImageLayout::Readonly);
            bindGroup.SetMultiscatteringLut({.Image = resources.GetTexture(passData.MultiscatteringLut)}, 
                ImageLayout::Readonly);
            bindGroup.SetSkyViewLut({.Image = lutTexture}, ImageLayout::General);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        }).Data;
}
