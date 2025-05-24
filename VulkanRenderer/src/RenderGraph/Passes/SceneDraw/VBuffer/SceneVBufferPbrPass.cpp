#include "SceneVBufferPbrPass.h"

#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneVbufferPbrUgbBindGroup.generated.h"
#include "Rendering/Image/ImageUtility.h"
#include "Rendering/Shader/ShaderOverrides.h"
#include "Scene/SceneGeometry.h"

Passes::SceneVBufferPbr::PassData& Passes::SceneVBufferPbr::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource VisibilityTexture{};
        Resource Camera{};
        Resource UGB{};
        Resource Indices{};
        Resource Commands{};
        Resource Objects{};
        SceneLightResources Light{};
        SSAOData SSAO{};
        IBLData IBL{};
        Resource ShadingSettings{};
        Resource Clusters{};
        Resource Tiles{};
        Resource ZBins{};
        CsmData CsmData{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Visibility.IBL.Setup")

            bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            bool useTiled = !useHybrid && info.Tiles.IsValid();
            bool useClustered = !useHybrid &&  info.Clusters.IsValid();

            graph.SetShader("scene-vbuffer-pbr-ugb"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{
                        "MAX_REFLECTION_LOD"_hsv,
                        (f32)Images::mipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION})},
                    ShaderSpecialization{"USE_TILED_LIGHTING"_hsv, useTiled},
                    ShaderSpecialization{"USE_CLUSTERED_LIGHTING"_hsv, useClustered},
                    ShaderSpecialization{"USE_HYBRID_LIGHTING"_hsv, useHybrid}});

            passData.Commands = graph.Import("Commands"_hsv, info.Geometry->Commands.Buffer);
            passData.Commands = graph.ReadBuffer(passData.Commands, Pixel | Storage);

            passData.Objects = graph.Import("Objects"_hsv, info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.ReadBuffer(passData.Objects, Pixel | Storage);

            passData.UGB = graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.ReadBuffer(passData.UGB, Pixel | Storage);

            passData.Indices = graph.Import("Indices"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices));
            passData.Indices = graph.ReadBuffer(passData.Indices, Pixel | Storage);

            Resource color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = info.VisibilityTexture,
                    .Format = Format::RGBA16_FLOAT});
            passData.Color = graph.RenderTarget(color, {.OnLoad = AttachmentLoad::Clear});

            passData.Light = RgUtils::readSceneLight(*info.Lights, graph, Pixel);
            if (info.Clusters.IsValid())
                passData.Clusters = graph.ReadBuffer(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.ReadBuffer(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.ReadBuffer(info.ZBins, Pixel | Storage);
            }
            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);
            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);

            if (info.CsmData.ShadowMap.IsValid())
                passData.CsmData = RgUtils::readCsmData(info.CsmData, graph, Pixel);

            passData.VisibilityTexture = graph.ReadImage(info.VisibilityTexture, Pixel | Sampled);
            
            passData.Camera = graph.ReadBuffer(info.Camera, Pixel | Uniform);
            
            auto& globalResources = graph.GetGlobalResources();
            passData.ShadingSettings = graph.ReadBuffer(globalResources.ShadingSettings, Pixel | Uniform);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("PBR Visibility pass")
            GPU_PROFILE_FRAME("PBR Visibility pass")

            const Shader& shader = graph.GetShader();
            SceneVbufferPbrUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera(graph.GetBufferBinding(passData.Camera));
            bindGroup.SetUGB(graph.GetBufferBinding(passData.UGB));
            bindGroup.SetIndices(graph.GetBufferBinding(passData.Indices));
            bindGroup.SetCommands(graph.GetBufferBinding(passData.Commands));
            bindGroup.SetObjects(graph.GetBufferBinding(passData.Objects));
            bindGroup.SetVisibilityTexture(graph.GetImageBinding(passData.VisibilityTexture));
            RgUtils::updateSceneLightBindings(bindGroup, graph, passData.Light);
            RgUtils::updateSSAOBindings(bindGroup, graph, passData.SSAO);
            RgUtils::updateIBLBindings(bindGroup, graph, passData.IBL);
            bindGroup.SetShading(graph.GetBufferBinding(passData.ShadingSettings));
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
            cmd.Draw({.VertexCount = 3});
        });
}
