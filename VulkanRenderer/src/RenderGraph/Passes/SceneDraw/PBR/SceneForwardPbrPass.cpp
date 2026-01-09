#include "rendererpch.h"

#include "SceneForwardPbrPass.h"

#include "RenderGraph/Passes/Generated/SceneDrawForwardPbrBindGroupRG.generated.h"
#include "Rendering/Image/ImageUtility.h"
#include "Scene/Scene.h"

Passes::SceneForwardPbr::PassData& Passes::SceneForwardPbr::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneDrawForwardPbrBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneForwardPbr.Setup")

            const bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            const bool useTiled = !useHybrid && info.Tiles.IsValid();
            const bool useClustered = !useHybrid && info.Clusters.IsValid();

            auto variant = SceneDrawForwardPbrBindGroupRG::Variants::Hybrid;
            if (useTiled)
                variant = SceneDrawForwardPbrBindGroupRG::Variants::Tiled;
            if (useClustered)
                variant = SceneDrawForwardPbrBindGroupRG::Variants::Clustered;

            const ShaderOverrides defaultOverrides(
                ShaderDynamicSpecializations(
                    ShaderSpecialization{"MAX_REFLECTION_LOD"_hsv, (f32)Images::mipmapCount(
                            glm::uvec2(graph.GetImageDescription(info.IBL.PrefilterEnvironment).Width))}
            ));
            
            passData.BindGroup = SceneDrawForwardPbrBindGroupRG(graph, variant, info.CommonOverrides.has_value() ?
                defaultOverrides.OverrideBy({*info.CommonOverrides, *info.DrawInfo.BucketOverrides}) :
                defaultOverrides.OverrideBy(*info.DrawInfo.BucketOverrides));

            passData.Resources.InitFrom(info.DrawInfo, graph);
            passData.BindGroup.SetResourcesUgb(graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes)));
            passData.BindGroup.SetResourcesRenderObjects(graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer));
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
            passData.BindGroup.SetResourcesCommands(graph.Import("Commands"_hsv, info.Geometry->Commands.Buffer));

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
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneForwardPbr")
            GPU_PROFILE_FRAME("Scene.SceneForwardPbr")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)
            });
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount
            });
        });
}
