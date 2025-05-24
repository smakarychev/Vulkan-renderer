#include "LightTilesBinPass.h"

#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightTilesBinBindGroup.generated.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::LightTilesBin::PassData& Passes::LightTilesBin::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin.Setup")

            graph.SetShader("light-tiles-bin"_hsv);

            passData.Depth = graph.ReadImage(info.Depth, Compute | Sampled);
            
            passData.Tiles = graph.ReadWriteBuffer(info.Tiles, Compute | Storage);
            
            passData.SceneLightResources = RgUtils::readSceneLight(*info.Light, graph, Compute);

            auto& globalResources = graph.GetGlobalResources();
            passData.Camera = graph.ReadBuffer(globalResources.PrimaryCameraGPU, Compute | Uniform);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin")
            GPU_PROFILE_FRAME("Lights.Tiles.Bin")

            auto& depthDescription = graph.GetImageDescription(passData.Depth);
            
            const Shader& shader = graph.GetShader();
            LightTilesBinShaderBindGroup bindGroup(shader);
            
            bindGroup.SetDepth(graph.GetImageBinding(passData.Depth));
            bindGroup.SetTiles(graph.GetBufferBinding(passData.Tiles));
            bindGroup.SetPointLights(graph.GetBufferBinding(passData.SceneLightResources.PointLights));
            bindGroup.SetLightsInfo(graph.GetBufferBinding(passData.SceneLightResources.LightsInfo));
            bindGroup.SetCamera(graph.GetBufferBinding(passData.Camera));
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {glm::vec2{frameContext.Resolution}}});
            cmd.Dispatch({
                .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                .GroupSize = {8, 8, 1}});
        });
}