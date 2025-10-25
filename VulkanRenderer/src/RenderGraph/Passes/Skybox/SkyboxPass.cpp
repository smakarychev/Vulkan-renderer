#include "rendererpch.h"

#include "rendererpch.h"

#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SkyboxBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::Skybox::PassData& Passes::Skybox::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct ProjectionUBO
    {
        glm::mat4 ProjectionInverse{1.0f};
        glm::mat4 ViewInverse{1.0f};
    };

    struct PassDataPrivate : PassData
    {
        Resource ViewInfo{};
        Resource Skybox{};
        f32 LodBias{0.0f};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Skybox.Setup")

            graph.SetShader("skybox"_hsv);
            
            passData.Color = RgUtils::ensureResource(info.Color, graph, "Color"_hsv,
                RGImageDescription{
                    .Width = (f32)info.Resolution.x,
                    .Height = (f32)info.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            ASSERT(info.Depth.IsValid(), "Depth has to be provided")

            const Resource skybox = info.SkyboxResource.IsValid() ?
                info.SkyboxResource : graph.Import("Skybox"_hsv, info.SkyboxTexture, ImageLayout::Readonly);
      
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Pixel | Uniform);
            passData.Skybox = graph.ReadImage(skybox, Pixel | Sampled);
            passData.Color = graph.RenderTarget(passData.Color, {});
            passData.Depth = graph.DepthStencilTarget(info.Depth, {});

            passData.LodBias = info.LodBias;
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Skybox")
            GPU_PROFILE_FRAME("Skybox")

            const Shader& shader = graph.GetShader();
            SkyboxShaderBindGroup bindGroup(shader);

            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetSkybox(graph.GetImageBinding(passData.Skybox));
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {passData.LodBias}});
            cmd.Draw({.VertexCount = 6});
        });
}
