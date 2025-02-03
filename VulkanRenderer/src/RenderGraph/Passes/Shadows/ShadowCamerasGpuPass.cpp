#include "ShadowCamerasGpuPass.h"

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/Generated/CreateShadowCamerasBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::ShadowCamerasGpu::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depthMinMax,
    RG::Resource primaryCamera, const glm::vec3& lightDirection)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU.Setup")
            
            graph.SetShader("create-shadow-cameras.shader");

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
            CreateShadowCamerasShaderBindGroup bindGroup(shader);

            bindGroup.SetMinMax({.Buffer = resources.GetBuffer(passData.DepthMinMax)});
            bindGroup.SetCsmData({.Buffer = resources.GetBuffer(passData.CsmDataOut)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.PrimaryCamera)});

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

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            cmd.Dispatch({
                .Invocations = {SHADOW_CASCADES, 1, 1},
                .GroupSize = {MAX_SHADOW_CASCADES, 1, 1}});
        });
}
