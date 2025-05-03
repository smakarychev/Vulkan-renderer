#include "VisualizeLightTiles.h"

#include "Light/LightZBinner.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/LightTilesVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesVisualize::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Color{};
        Resource Tiles{};
        Resource Camera{};
        Resource ZBins{};
        Resource Depth{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize.Setup")

            graph.SetShader("light-tiles-visualize"_hsv);

            auto& globalResources = graph.GetGlobalResources();

            passData.Color = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.ZBins = {};
            if (info.Bins.IsValid())
                passData.ZBins = graph.Read(info.Bins, Pixel | Storage);
            
            passData.Color = graph.RenderTarget(passData.Color, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Depth = graph.Read(info.Depth, Pixel | Sampled);
            passData.Tiles = graph.Read(info.Tiles, Pixel | Storage);

            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);

            PassData passDataPublic = {};
            passDataPublic.Color = passData.Color;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize")
            GPU_PROFILE_FRAME("Lights.Tiles.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            LightTilesVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetDepth({.Image = resources.GetTexture(passData.Depth)}, ImageLayout::Readonly);
            bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            bool useZBins = passData.ZBins.IsValid();
            if (useZBins)
                bindGroup.SetZbins({.Buffer = resources.GetBuffer(passData.ZBins)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {useZBins}});
            cmd.Draw({.VertexCount = 3});
        });
}
