#include "SceneForwardPbrPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneForwardPbrBindGroup.generated.h"
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
        Resource ShadingSettings{};
        Resource Clusters{};
        Resource Tiles{};
        Resource ZBins{};
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
                        (f32)Images::mipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION})},
                    ShaderSpecialization{"USE_TILED_LIGHTING"_hsv, useTiled},
                    ShaderSpecialization{"USE_CLUSTERED_LIGHTING"_hsv, useClustered},
                    ShaderSpecialization{"USE_HYBRID_LIGHTING"_hsv, useHybrid}));
            graph.SetShader("scene-forward-pbr"_hsv, info.CommonOverrides.has_value() ?
                defaultOverrides.OverrideBy({*info.CommonOverrides, *info.DrawInfo.BucketOverrides}) :
                defaultOverrides.OverrideBy(*info.DrawInfo.BucketOverrides));

            passData.Resources.CreateFrom(info.DrawInfo, graph);

            passData.UGB = graph.AddExternal("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.Read(passData.Objects, Vertex | Pixel | Storage);

            passData.Light = RgUtils::readSceneLight(*info.Lights, graph, Pixel);

            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);

            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);

            // todo: remove once this is united with view
            auto& globalResources = graph.GetGlobalResources();
            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);

            if (info.Clusters.IsValid())
                passData.Clusters = graph.Read(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.Read(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.Read(info.ZBins, Pixel | Storage);
            }
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.SceneForwardPbr")
            GPU_PROFILE_FRAME("Scene.SceneForwardPbr")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneForwardPbrShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Resources.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Resources.Draws)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});
            RgUtils::updateSceneLightBindings(bindGroup, resources, passData.Light);
            RgUtils::updateSSAOBindings(bindGroup, resources, passData.SSAO);
            RgUtils::updateIBLBindings(bindGroup, resources, passData.IBL);
            bindGroup.SetShading({.Buffer = resources.GetBuffer(passData.ShadingSettings)});
            if (passData.Clusters.IsValid())
                bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            if (passData.Tiles.IsValid())
            {
                bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
                bindGroup.SetZbins({.Buffer = resources.GetBuffer(passData.ZBins)});
            }

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = resources.GetBuffer(passData.Resources.Draws),
                .CountBuffer = resources.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        }).Data;
}
