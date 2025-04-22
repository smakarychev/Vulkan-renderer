#include "PbrVisibilityBufferIBLPass.h"

#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/PbrVisibilityIblBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Pbr::VisibilityIbl::addToGraph(StringId name, RG::Graph& renderGraph,
    const PbrVisibilityBufferExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Visibility.IBL.Setup")

            bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            bool useTiled = !useHybrid && info.Tiles.IsValid();
            bool useClustered = !useHybrid &&  info.Clusters.IsValid();

            graph.SetShader("pbr-visibility-ibl.shader",
                ShaderSpecializations{
                    ShaderSpecialization{
                        "MAX_REFLECTION_LOD"_hsv,
                        (f32)Images::mipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION})},
                    ShaderSpecialization{"USE_TILED_LIGHTING"_hsv, useTiled},
                    ShaderSpecialization{"USE_CLUSTERED_LIGHTING"_hsv, useClustered},
                    ShaderSpecialization{"USE_HYBRID_LIGHTING"_hsv, useHybrid}});

            passData.Commands = graph.AddExternal("Commands"_hsv, info.Geometry->GetCommandsBuffer());
            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->GetRenderObjectsBuffer());
            auto& attributes = info.Geometry->GetAttributeBuffers();
            passData.Positions = graph.AddExternal("Positions"_hsv, attributes.Positions);
            passData.Normals = graph.AddExternal("Normals"_hsv, attributes.Normals);
            passData.Tangents = graph.AddExternal("Tangents"_hsv, attributes.Tangents);
            passData.UVs = graph.AddExternal("UVs"_hsv, attributes.UVs);
            passData.Indices = graph.AddExternal("Indices"_hsv, attributes.Indices);

            const TextureDescription& visibilityDescription =
                Resources(graph).GetTextureDescription(info.VisibilityTexture);
            
            Resource color = RgUtils::ensureResource(info.ColorIn, graph, "Color"_hsv,
                GraphTextureDescription{
                   .Width = visibilityDescription.Width,
                   .Height = visibilityDescription.Height,
                   .Format = Format::RGBA16_FLOAT});

            passData.LightsResources = RgUtils::readSceneLight(*info.SceneLights, graph, Pixel);
            if (info.Clusters.IsValid())
                passData.Clusters = graph.Read(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.Read(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.Read(info.ZBins, Pixel | Storage);
            }
            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);
            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);
            passData.CSMData = RgUtils::readCSMData(info.CSMData, graph, Pixel);

            auto& globalResources = graph.GetGlobalResources();
            
            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);
            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);
            passData.Commands = graph.Read(passData.Commands, Pixel | Storage);
            passData.Objects = graph.Read(passData.Objects, Pixel | Storage);
            passData.Positions = graph.Read(passData.Positions, Pixel | Storage);
            passData.Normals = graph.Read(passData.Normals, Pixel | Storage);
            passData.Tangents = graph.Read(passData.Tangents, Pixel | Storage);
            passData.UVs = graph.Read(passData.UVs, Pixel | Storage);
            passData.Indices = graph.Read(passData.Indices, Pixel | Storage);

            passData.ColorOut = graph.RenderTarget(color,
                info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("PBR Visibility pass")
            GPU_PROFILE_FRAME("PBR Visibility pass")

            Texture visibility = resources.GetTexture(passData.VisibilityTexture);
            Buffer cameraBuffer = resources.GetBuffer(passData.Camera);
            Buffer shadingSettings = resources.GetBuffer(passData.ShadingSettings);

            Buffer commands = resources.GetBuffer(passData.Commands);
            Buffer objects = resources.GetBuffer(passData.Objects);
            Buffer positions = resources.GetBuffer(passData.Positions);
            Buffer normals = resources.GetBuffer(passData.Normals);
            Buffer tangents = resources.GetBuffer(passData.Tangents);
            Buffer uvs = resources.GetBuffer(passData.UVs);
            Buffer indices = resources.GetBuffer(passData.Indices);

            const Shader& shader = resources.GetGraph()->GetShader();
            PbrVisibilityIblShaderBindGroup bindGroup(shader);
            
            bindGroup.SetVisibilityTexture({.Image = visibility}, ImageLayout::Readonly);
            RgUtils::updateSceneLightBindings(bindGroup, resources, passData.LightsResources);
            if (passData.Clusters.IsValid())
                bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            if (passData.Tiles.IsValid())
            {
                bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
                bindGroup.SetZbins({.Buffer = resources.GetBuffer(passData.ZBins)});
            }
            RgUtils::updateIBLBindings(bindGroup, resources, passData.IBL);
            RgUtils::updateSSAOBindings(bindGroup, resources, passData.SSAO);
            RgUtils::updateCSMBindings(bindGroup, resources, passData.CSMData);
            bindGroup.SetCamera({.Buffer = cameraBuffer});
            bindGroup.SetShading({.Buffer = shadingSettings});
            bindGroup.SetCommands({.Buffer = commands});
            bindGroup.SetObjects({.Buffer = objects});
            bindGroup.SetPositions({.Buffer = positions});
            bindGroup.SetNormals({.Buffer = normals});
            bindGroup.SetTangents({.Buffer = tangents});
            bindGroup.SetUv({.Buffer = uvs});
            bindGroup.SetIndices({.Buffer = indices});
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.Draw({.VertexCount = 3});
        });

    return pass;
}
