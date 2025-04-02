#include "VisualizeLightTiles.h"

#include "Light/LightZBinner.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/LightTilesVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesVisualize::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource tiles,
    RG::Resource depth, RG::Resource bins)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize.Setup")

            graph.SetShader("light-tiles-visualize.shader");

            auto& globalResources = graph.GetGlobalResources();

            passData.ColorOut = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.ZBins = {};
            if (bins.IsValid())
                passData.ZBins = graph.Read(bins, Pixel | Storage);
            
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Depth = graph.Read(depth, Pixel | Sampled);
            passData.Tiles = graph.Read(tiles, Pixel | Storage);

            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize")
            GPU_PROFILE_FRAME("Lights.Tiles.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            LightTilesVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetDepth({.Image = resources.GetTexture(depth)}, ImageLayout::Readonly);
            bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            bool useZBins = passData.ZBins.IsValid();
            if (useZBins)
                bindGroup.SetZbins({.Buffer = resources.GetBuffer(passData.ZBins)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {useZBins}});
            cmd.Draw({.VertexCount = 3});
        });
}
