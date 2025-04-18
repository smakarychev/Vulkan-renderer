#include "LightTilesBinPass.h"

#include <tracy/Tracy.hpp>

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightTilesBinBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::LightTilesBin::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource tiles,
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

            auto&& [depthTexture, depthDescription] = resources.GetTextureWithDescription(depth);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            LightTilesBinShaderBindGroup bindGroup(shader);
            
            bindGroup.SetDepth({.Image = depthTexture}, ImageLayout::Readonly);
            bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});
            bindGroup.SetPointLights({.Buffer = resources.GetBuffer(passData.SceneLightResources.PointLights)});
            bindGroup.SetLightsInfo({.Buffer = resources.GetBuffer(passData.SceneLightResources.LightsInfo)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {glm::vec2{frameContext.Resolution}}});
            cmd.Dispatch({
                .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                .GroupSize = {8, 8, 1}});
        });
}
