#include "SceneVBufferPbrPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneVbufferPbrUgbBindGroup.generated.h"
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

            passData.Commands = graph.AddExternal("Commands"_hsv, info.Geometry->Commands.Buffer);
            passData.Commands = graph.Read(passData.Commands, Pixel | Storage);

            passData.Objects = graph.AddExternal("Objects"_hsv, info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.Read(passData.Objects, Pixel | Storage);

            passData.UGB = graph.AddExternal("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Pixel | Storage);

            passData.Indices = graph.AddExternal("Indices"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices));
            passData.Indices = graph.Read(passData.Indices, Pixel | Storage);

            const TextureDescription& visibilityDescription =
                Resources(graph).GetTextureDescription(info.VisibilityTexture);
            Resource color = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                   .Width = visibilityDescription.Width,
                   .Height = visibilityDescription.Height,
                   .Format = Format::RGBA16_FLOAT});
            passData.Color = graph.RenderTarget(color, AttachmentLoad::Clear, AttachmentStore::Store);

            passData.Light = RgUtils::readSceneLight(*info.Lights, graph, Pixel);
            if (info.Clusters.IsValid())
                passData.Clusters = graph.Read(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.Read(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.Read(info.ZBins, Pixel | Storage);
            }
            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);
            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);

            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            
            passData.Camera = graph.Read(info.Camera, Pixel | Uniform);
            
            auto& globalResources = graph.GetGlobalResources();
            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("PBR Visibility pass")
            GPU_PROFILE_FRAME("PBR Visibility pass")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneVbufferPbrUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetIndices({.Buffer = resources.GetBuffer(passData.Indices)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Commands)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});
            bindGroup.SetVisibilityTexture({.Image = resources.GetTexture(passData.VisibilityTexture)},
                ImageLayout::Readonly);
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
            cmd.Draw({.VertexCount = 3});
        }).Data;
}
