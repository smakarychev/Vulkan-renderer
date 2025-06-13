#include "SceneForwardPbrPass.h"

#include "ViewInfoGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneForwardPbrBindGroup.generated.h"
#include "Rendering/Image/ImageUtility.h"
#include "Scene/Scene.h"

Passes::SceneForwardPbr::PassData& Passes::SceneForwardPbr::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource UGB{};
        Resource Objects{};
        SceneLightResources Light{};
        SSAOData SSAO{};
        IBLData IBL{};
        Resource Clusters{};
        Resource Tiles{};
        Resource ZBins{};
        CsmData CsmData{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneForwardPbr.Setup")

            const bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            const bool useTiled = !useHybrid && info.Tiles.IsValid();
            const bool useClustered = !useHybrid &&  info.Clusters.IsValid();
            const ShaderOverrides defaultOverrides(
                ShaderDynamicSpecializations(
                    ShaderSpecialization{
                        "MAX_REFLECTION_LOD"_hsv,
                        (f32)Images::mipmapCount(
                            glm::uvec2(graph.GetImageDescription(info.IBL.PrefilterEnvironment).Width))},
                    ShaderSpecialization{"USE_TILED_LIGHTING"_hsv, useTiled},
                    ShaderSpecialization{"USE_CLUSTERED_LIGHTING"_hsv, useClustered},
                    ShaderSpecialization{"USE_HYBRID_LIGHTING"_hsv, useHybrid}));
            graph.SetShader("scene-forward-pbr"_hsv, info.CommonOverrides.has_value() ?
                defaultOverrides.OverrideBy({*info.CommonOverrides, *info.DrawInfo.BucketOverrides}) :
                defaultOverrides.OverrideBy(*info.DrawInfo.BucketOverrides));

            passData.Resources.CreateFrom(info.DrawInfo, graph);

            passData.UGB = graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.ReadBuffer(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.ReadBuffer(passData.Objects, Vertex | Pixel | Storage);

            passData.Light = RgUtils::readSceneLight(*info.Light, graph, Pixel);

            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);

            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);

            if (info.Clusters.IsValid())
                passData.Clusters = graph.ReadBuffer(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.ReadBuffer(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.ReadBuffer(info.ZBins, Pixel | Storage);
            }

            if (info.CsmData.ShadowMap.IsValid())
                passData.CsmData = RgUtils::readCsmData(info.CsmData, graph, Pixel);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneForwardPbr")
            GPU_PROFILE_FRAME("Scene.SceneForwardPbr")

            const Shader& shader = graph.GetShader();
            SceneForwardPbrShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.Resources.ViewInfo));
            bindGroup.SetUGB(graph.GetBufferBinding(passData.UGB));
            bindGroup.SetCommands(graph.GetBufferBinding(passData.Resources.Draws));
            bindGroup.SetObjects(graph.GetBufferBinding(passData.Objects));
            RgUtils::updateSceneLightBindings(bindGroup, graph, passData.Light);
            RgUtils::updateSSAOBindings(bindGroup, graph, passData.SSAO);
            RgUtils::updateIBLBindings(bindGroup, graph, passData.IBL);
            if (passData.Clusters.IsValid())
                bindGroup.SetClusters(graph.GetBufferBinding(passData.Clusters));
            if (passData.Tiles.IsValid())
            {
                bindGroup.SetTiles(graph.GetBufferBinding(passData.Tiles));
                bindGroup.SetZbins(graph.GetBufferBinding(passData.ZBins));
            }
            if (passData.CsmData.ShadowMap.IsValid())
                RgUtils::updateCsmBindings(bindGroup, graph, passData.CsmData);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        });
}
