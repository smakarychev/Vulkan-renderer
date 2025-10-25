#include "rendererpch.h"

#include "DiffuseIrradianceSHPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceShBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::DiffuseIrradianceSH::PassData& Passes::DiffuseIrradianceSH::addToGraph(StringId name, RG::Graph& renderGraph,
    Texture cubemap, Buffer irradianceSH, bool realTime)
{
    return addToGraph(name, renderGraph,
        renderGraph.Import("CubemapTexture.Import"_hsv, cubemap, ImageLayout::Readonly),
        renderGraph.Import("DiffuseIrradianceSH.Import"_hsv, irradianceSH), realTime);
}

Passes::DiffuseIrradianceSH::PassData& Passes::DiffuseIrradianceSH::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, Buffer irradianceSH, bool realTime)
{
    return addToGraph(name, renderGraph, cubemap,
        renderGraph.Import("DiffuseIrradianceSH.Import"_hsv, irradianceSH), realTime);
}

Passes::DiffuseIrradianceSH::PassData& Passes::DiffuseIrradianceSH::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, RG::Resource irradianceSH, bool realTime)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Setup")

            graph.SetShader("diffuse-irradiance-sh"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"REAL_TIME"_hsv, realTime}});
            
            passData.DiffuseIrradiance = graph.WriteBuffer(irradianceSH, Compute | Storage);
            passData.CubemapTexture = graph.ReadImage(cubemap, Compute | Sampled);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH")

            auto& cubemapDescription = graph.GetImageDescription(passData.CubemapTexture);

            const Shader& shader = graph.GetShader();
            DiffuseIrradianceShShaderBindGroup bindGroup(shader);

            bindGroup.SetSh(graph.GetBufferBinding(passData.DiffuseIrradiance));
            bindGroup.SetEnv(graph.GetImageBinding(passData.CubemapTexture));

            const u32 realTimeMipmapsCount = (u32)std::log(16.0);
            const u32 targetMipmap = realTime ?
                std::min(cubemapDescription.Mipmaps, (i8)realTimeMipmapsCount) - realTimeMipmapsCount :
                0;
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {targetMipmap}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1}});
        });
}
