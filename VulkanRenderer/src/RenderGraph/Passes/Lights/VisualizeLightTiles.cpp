#include "VisualizeLightTiles.h"

#include "Light/LightZBinner.h"
#include "RenderGraph/RenderGraph.h"
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

            graph.SetShader("../assets/shaders/light-tiles-visualize.shader");

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
            auto pipeline = shader.Pipeline();
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_depth", resources.GetTexture(depth).BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_tiles", resources.GetBuffer(passData.Tiles).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.Camera).BindingInfo());

            bool useZBins = passData.ZBins.IsValid();
            if (useZBins)
                resourceDescriptors.UpdateBinding("u_zbins", resources.GetBuffer(passData.ZBins).BindingInfo());

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindGraphics(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), useZBins);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
}
