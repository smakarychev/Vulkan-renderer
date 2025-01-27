#include "VisualizeLightTiles.h"

#include "Light/LightZBinner.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/LightTilesVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesVisualize::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource tiles,
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

            passData.ColorOut = graph.CreateResource(std::string{name} + ".Color",
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
            bindGroup.SetDepth(resources.GetTexture(depth).BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetTiles(resources.GetBuffer(passData.Tiles).BindingInfo());
            bindGroup.SetCamera(resources.GetBuffer(passData.Camera).BindingInfo());

            bool useZBins = passData.ZBins.IsValid();
            if (useZBins)
                bindGroup.SetZbins(resources.GetBuffer(passData.ZBins).BindingInfo());

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::PushConstants(cmd, shader.GetLayout(), useZBins);
            RenderCommand::Draw(cmd, 3);
        });
}
