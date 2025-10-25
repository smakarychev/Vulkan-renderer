#include "rendererpch.h"

#include "LightTilesSetupPass.h"

#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "Light/Light.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/LightTilesSetupBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::LightTilesSetup::PassData& Passes::LightTilesSetup::addToGraph(StringId name, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup.Setup")

            graph.SetShader("light-tiles-setup"_hsv);

            auto& globalResources = graph.GetGlobalResources();

            const glm::uvec2 bins = glm::ceil(
                glm::vec2{globalResources.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            passData.Tiles = graph.Create("Tiles"_hsv, RGBufferDescription{
                .SizeBytes = (u64)(bins.x * bins.y) * sizeof(LightTile)});
            passData.Tiles = graph.WriteBuffer(passData.Tiles, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup")
            GPU_PROFILE_FRAME("Lights.Tiles.Setup")

            const Shader& shader = graph.GetShader();
            LightTilesSetupShaderBindGroup bindGroup(shader);
            
            bindGroup.SetTiles(graph.GetBufferBinding(passData.Tiles));

            struct PushConstant
            {
                glm::vec2 RenderSize;
                f32 Near;
                f32 Far;
                glm::mat4 ProjectionInverse;
            };
            PushConstant pushConstant = {
                .RenderSize = frameContext.Resolution,
                .Near = frameContext.PrimaryCamera->GetNear(),
                .Far = *CVars::Get().GetF32CVar("Renderer.Limits.MaxLightCullDistance"_hsv),
                .ProjectionInverse = glm::inverse(frameContext.PrimaryCamera->GetProjection())};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            glm::uvec2 bins = glm::ceil(
                glm::vec2{frameContext.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            cmd.Dispatch({
                .Invocations = {bins.x, bins.y, 1},
                .GroupSize = {1, 1, 1}});
        });
}
