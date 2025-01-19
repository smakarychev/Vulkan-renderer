#include "LightTilesBinPass.h"

#include <tracy/Tracy.hpp>

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
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

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            const Texture& depthTexture = resources.GetTexture(depth);

            resourceDescriptors.UpdateBinding("u_depth", depthTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_tiles", resources.GetBuffer(passData.Tiles).BindingInfo());
            resourceDescriptors.UpdateBinding("u_point_lights",
                resources.GetBuffer(passData.SceneLightResources.PointLights).BindingInfo());
            resourceDescriptors.UpdateBinding("u_lights_info",
                resources.GetBuffer(passData.SceneLightResources.LightsInfo).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.Camera).BindingInfo());
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindCompute(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), glm::vec2{frameContext.Resolution});
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Dispatch(cmd,
                {depthTexture.Description().Width, depthTexture.Description().Height, 1},
                {8, 8, 1});
        });
}
