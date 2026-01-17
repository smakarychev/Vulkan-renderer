#include "rendererpch.h"

#include "DiffuseIrradianceSHPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceShOfflineBindGroupRG.generated.h"
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

namespace 
{
Passes::DiffuseIrradianceSH::PassData& addOffline(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, RG::Resource irradianceSH)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<Passes::DiffuseIrradianceSH::PassData, DiffuseIrradianceShOfflineBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Offline.Setup")

            passData.BindGroup = DiffuseIrradianceShOfflineBindGroupRG(graph);

            passData.DiffuseIrradiance = passData.BindGroup.SetResourcesIrradiance(irradianceSH);
            passData.CubemapTexture = passData.BindGroup.SetResourcesEnvironment(cubemap);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Offline")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH.Offline")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {0.0f}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1}});
        });
}

Passes::DiffuseIrradianceSH::PassData& addRealtime(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, RG::Resource irradianceSH)
{
    using namespace RG;
    using PassDataBind =
        PassDataWithBind<Passes::DiffuseIrradianceSH::PassData, DiffuseIrradianceShRealtimeBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Setup")

            passData.BindGroup = DiffuseIrradianceShRealtimeBindGroupRG(graph);

            passData.DiffuseIrradiance = passData.BindGroup.SetResourcesIrradiance(irradianceSH);
            passData.CubemapTexture = passData.BindGroup.SetResourcesEnvironment(cubemap);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH")

            auto& cubemapDescription = graph.GetImageDescription(passData.CubemapTexture);
            const u32 realTimeMipmapsCount = (u32)std::log(16.0);
            const f32 targetMipmap =
                (f32)std::min(cubemapDescription.Mipmaps, (i8)realTimeMipmapsCount) - (f32)realTimeMipmapsCount;
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {targetMipmap}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1}});
        });
}
}

Passes::DiffuseIrradianceSH::PassData& Passes::DiffuseIrradianceSH::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, RG::Resource irradianceSH, bool realTime)
{
    return realTime ?
        addRealtime(name, renderGraph, cubemap, irradianceSH) :
        addOffline(name, renderGraph, cubemap, irradianceSH);
}