#include "LightTilesSetupPass.h"

#include "Core/Camera.h"
#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
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

            glm::uvec2 bins = glm::ceil(
                glm::vec2{globalResources.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            passData.Tiles = graph.CreateResource("Tiles"_hsv, GraphBufferDescription{
                .SizeBytes = (u64)(bins.x * bins.y) * sizeof(LightTile)});
            passData.Tiles = graph.Write(passData.Tiles, Compute | Storage);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup")
            GPU_PROFILE_FRAME("Lights.Tiles.Setup")

            const Shader& shader = resources.GetGraph()->GetShader();
            LightTilesSetupShaderBindGroup bindGroup(shader);
            
            bindGroup.SetTiles({.Buffer = resources.GetBuffer(passData.Tiles)});

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
                .Far = frameContext.PrimaryCamera->GetFar(),
                .ProjectionInverse = glm::inverse(frameContext.PrimaryCamera->GetProjection())};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            glm::uvec2 bins = glm::ceil(
                glm::vec2{frameContext.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            cmd.Dispatch({
                .Invocations = {bins.x, bins.y, 1},
                .GroupSize = {1, 1, 1}});
        }).Data;
}
