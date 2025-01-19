#include "LightTilesSetupPass.h"

#include "Core/Camera.h"
#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::LightTilesSetup::addToGraph(std::string_view name, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup.Setup")

            graph.SetShader("light-tiles-setup.shader");

            auto& globalResources = graph.GetGlobalResources();

            glm::uvec2 bins = glm::ceil(
                glm::vec2{globalResources.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            passData.Tiles = graph.CreateResource(std::format("{}.Tiles", name), GraphBufferDescription{
                .SizeBytes = bins.x * bins.y * sizeof(LightTile)});
            passData.Tiles = graph.Write(passData.Tiles, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup")
            GPU_PROFILE_FRAME("Lights.Tiles.Setup")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_tiles", resources.GetBuffer(passData.Tiles).BindingInfo());

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

            auto& cmd = frameContext.Cmd;
            RenderCommand::BindCompute(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            glm::uvec2 bins = glm::ceil(
                glm::vec2{frameContext.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            RenderCommand::Dispatch(cmd,
                {bins.x, bins.y, 1},
                {1, 1, 1});
        });
}
