#include "rendererpch.h"

#include "SceneVBufferPbrPass.h"

#include "RenderGraph/Passes/Generated/SceneVbufferPbrBindGroupRG.generated.h"
#include "Rendering/Image/ImageUtility.h"
#include "Scene/SceneGeometry.h"
#include "Scene/SceneLight.h"

Passes::SceneVBufferPbr::PassData& Passes::SceneVBufferPbr::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneVbufferPbrBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Visibility.Setup")

            const bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            const bool useTiled = !useHybrid && info.Tiles.IsValid();
            const bool useClustered = !useHybrid && info.Clusters.IsValid();

            auto variant = SceneVbufferPbrBindGroupRG::Variants::Hybrid;
            if (useTiled)
                variant = SceneVbufferPbrBindGroupRG::Variants::Tiled;
            if (useClustered)
                variant = SceneVbufferPbrBindGroupRG::Variants::Clustered;

            passData.BindGroup = SceneVbufferPbrBindGroupRG(graph, variant, ShaderSpecializations(
                ShaderSpecialization{
                    "MAX_REFLECTION_LOD"_hsv, (f32)Images::mipmapCount(
                        glm::uvec2(graph.GetImageDescription(info.IBL.PrefilterEnvironment).Width))
                }
            ));

            passData.BindGroup.SetResourcesVbuffer(info.VisibilityTexture);
            passData.BindGroup.SetResourcesUgb(graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes)));
            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesCommands(graph.Import("Commands"_hsv, info.Geometry->Commands.Buffer));
            passData.BindGroup.SetResourcesRenderObjects(graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer));
            passData.BindGroup.SetResourcesIndices(graph.Import("Indices"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)));
            passData.BindGroup.SetResourcesCsmData(info.CsmData.CsmInfo);
            passData.BindGroup.SetResourcesCsmTexture(info.CsmData.ShadowMap);
            passData.BindGroup.SetResourcesSsao(info.SSAO.SSAO);
            passData.BindGroup.SetResourcesDirectionalLights(graph.Import("Light.Directional"_hsv,
                info.Light->GetBuffers().DirectionalLights));
            passData.BindGroup.SetResourcesPointLights(graph.Import("Light.Point"_hsv,
                info.Light->GetBuffers().PointLights));
            if (useTiled || useHybrid)
            {
                passData.BindGroup.SetResourcesLightZBins(info.ZBins);
                passData.BindGroup.SetResourcesLightTiles(info.Tiles);
            }
            if (useClustered || useHybrid)
            {
                passData.BindGroup.SetResourcesLightClusters(info.Clusters);
            }
            passData.BindGroup.SetResourcesPrefilteredEnvironment(info.IBL.PrefilterEnvironment);
            passData.BindGroup.SetResourcesBrdf(info.IBL.BRDF);
            passData.BindGroup.SetResourcesIrradianceSH(info.IBL.IrradianceSH);

            const Resource color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = info.VisibilityTexture,
                    .Format = SceneVbufferPbrBindGroupRG::GetColorAttachmentFormat()
                });
            passData.Color = graph.RenderTarget(color, {.OnLoad = AttachmentLoad::Clear});
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Pbr.Visibility")
            GPU_PROFILE_FRAME("Pbr.Visibility")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
