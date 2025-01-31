#include "LightTilesBinPass.h"

#include <tracy/Tracy.hpp>

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightTilesBinBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesBin::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource tiles,
    RG::Resource depth, const SceneLight& sceneLight)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin.Setup")

            graph.SetShader("light-tiles-bin.shader");

            passData.Depth = graph.Read(depth, Compute | Sampled);
            
            passData.Tiles = graph.Read(tiles, Compute | Storage);
            passData.Tiles = graph.Write(passData.Tiles, Compute | Storage);
            
            passData.SceneLightResources = RgUtils::readSceneLight(sceneLight, graph, Compute);

            auto& globalResources = graph.GetGlobalResources();
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Compute | Uniform);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin")
            GPU_PROFILE_FRAME("Lights.Tiles.Bin")

            const Texture& depthTexture = resources.GetTexture(depth);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            LightTilesBinShaderBindGroup bindGroup(shader);
            
            bindGroup.SetDepth(depthTexture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
            bindGroup.SetPointLights({.Buffer = resources.GetBuffer(passData.SceneLightResources.PointLights)});
            bindGroup.SetLightsInfo({.Buffer = resources.GetBuffer(passData.SceneLightResources.LightsInfo)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            
            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::PushConstants(cmd, shader.GetLayout(), glm::vec2{frameContext.Resolution});
            RenderCommand::Dispatch(cmd,
                {depthTexture.Description().Width, depthTexture.Description().Height, 1},
                {8, 8, 1});
        });
}
