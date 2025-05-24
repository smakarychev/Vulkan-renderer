#include "ShadowCamerasGpuPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/Generated/CreateShadowCamerasBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::ShadowCamerasGpu::PassData& Passes::ShadowCamerasGpu::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource depthMinMax, RG::Resource primaryCamera, const glm::vec3& lightDirection)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU.Setup")
            
            graph.SetShader("create-shadow-cameras"_hsv);

            Resource csmData = graph.Create("CSM.Data"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(CsmData)});

            passData.DepthMinMax = graph.ReadBuffer(depthMinMax, Compute | Uniform);
            passData.PrimaryCamera = graph.ReadBuffer(primaryCamera, Compute | Uniform);
            passData.CsmDataOut = graph.WriteBuffer(csmData, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU")
            GPU_PROFILE_FRAME("ShadowCameras.GPU")

            const Shader& shader = graph.GetShader();
            CreateShadowCamerasShaderBindGroup bindGroup(shader);

            bindGroup.SetMinMax(graph.GetBufferBinding(passData.DepthMinMax));
            bindGroup.SetCsmData(graph.GetBufferBinding(passData.CsmDataOut));
            bindGroup.SetCamera(graph.GetBufferBinding(passData.PrimaryCamera));

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
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            cmd.Dispatch({
                .Invocations = {SHADOW_CASCADES, 1, 1},
                .GroupSize = {MAX_SHADOW_CASCADES, 1, 1}});
        });
}
