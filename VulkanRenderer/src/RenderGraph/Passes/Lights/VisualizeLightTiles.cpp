#include "VisualizeLightTiles.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesVisualize::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource tiles)
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
            
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Tiles = graph.Read(tiles, Pixel | Storage);

            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize")
            GPU_PROFILE_FRAME("Lights.Tiles.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline();
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_tiles", resources.GetBuffer(passData.Tiles).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.Camera).BindingInfo());

            auto& cmd = frameContext.Cmd;
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
}
