#include "rendererpch.h"

#include "SkyboxPass.h"

#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneDrawSkyboxBindGroupRG.generated.h"

Passes::Skybox::PassData& Passes::Skybox::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneDrawSkyboxBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Skybox.Setup")

            passData.BindGroup = SceneDrawSkyboxBindGroupRG(graph);

            passData.Color = RgUtils::ensureResource(info.Color, graph, "Color"_hsv,
                RGImageDescription{
                    .Width = (f32)info.Resolution.x,
                    .Height = (f32)info.Resolution.y,
                    .Format = SceneDrawSkyboxBindGroupRG::GetColorAttachmentFormat()
            });
            ASSERT(info.Depth.IsValid(), "Depth has to be provided")

            const Resource skybox = info.SkyboxResource.IsValid() ?
                info.SkyboxResource : graph.Import("Skybox"_hsv, info.SkyboxTexture, ImageLayout::Readonly);

            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesSkybox(skybox);
            
            passData.Color = graph.RenderTarget(passData.Color, {});
            passData.Depth = graph.DepthStencilTarget(info.Depth, {});
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Skybox")
            GPU_PROFILE_FRAME("Skybox")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.PushConstants({
            	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
            	.Data = {info.LodBias}
            });
            cmd.Draw({.VertexCount = 6});
        });
}
