#include "ShadowCamerasGpuPass.h"

#include "RenderGraph/RGDrawResources.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::ShadowCamerasGpu::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depthMinMax,
    RG::Resource primaryCamera, const glm::vec3& lightDirection)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU.Setup")
            
            graph.SetShader("../assets/shaders/create-shadow-cameras.shader");

            Resource csmData = graph.CreateResource(std::format("{}.CSM.Data", name), GraphBufferDescription{
                .SizeBytes = sizeof(CSMData)});

            passData.DepthMinMax = graph.Read(depthMinMax, Compute | Uniform);
            passData.PrimaryCamera = graph.Read(primaryCamera, Compute | Uniform);
            passData.CsmDataOut = graph.Write(csmData, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU")
            GPU_PROFILE_FRAME("ShadowCameras.GPU")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_min_max", resources.GetBuffer(passData.DepthMinMax).BindingInfo());
            resourceDescriptors.UpdateBinding("u_csm_data", resources.GetBuffer(passData.CsmDataOut).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.PrimaryCamera).BindingInfo());

            struct PushConstant
            {
                u32 ShadowSize;
                u32 CascadeCount;
                glm::vec3 LightDirection;
            };
            PushConstant pushConstant = {
                .ShadowSize = SHADOW_MAP_RESOLUTION,
                .CascadeCount = SHADOW_CASCADES,
                .LightDirection = lightDirection};
            
            pipeline.BindCompute(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            RenderCommand::Dispatch(frameContext.Cmd,
                {SHADOW_CASCADES, 1, 1},
                {MAX_SHADOW_CASCADES, 1, 1});
        });
}
