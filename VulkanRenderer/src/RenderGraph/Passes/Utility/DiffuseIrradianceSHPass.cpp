#include "rendererpch.h"

#include "DiffuseIrradianceSHPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceShRealtimeBindGroupRG.generated.h"
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

    using PassDataBind = PassDataWithBind<PassData, DiffuseIrradianceShRealtimeBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Setup")

            passData.BindGroup = DiffuseIrradianceShRealtimeBindGroupRG(graph,
                graph.SetShader(realTime ? "diffuseIrradianceShRealtime"_hsv : "diffuseIrradianceShOffline"_hsv));

            passData.DiffuseIrradiance = passData.BindGroup.SetResourcesIrradiance(irradianceSH);
            passData.CubemapTexture = passData.BindGroup.SetResourcesEnvironment(cubemap);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH")

            auto& cubemapDescription = graph.GetImageDescription(passData.CubemapTexture);

            const u32 realTimeMipmapsCount = (u32)std::log(16.0);
            const f32 targetMipmap = realTime ?
                (f32)std::min(cubemapDescription.Mipmaps, (i8)realTimeMipmapsCount) - (f32)realTimeMipmapsCount :
                0.0f;
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {targetMipmap}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1}});
        });
}
