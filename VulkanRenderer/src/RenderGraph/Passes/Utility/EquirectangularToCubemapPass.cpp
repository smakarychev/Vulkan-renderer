#include "rendererpch.h"

#include "EquirectangularToCubemapPass.h"

#include "MipMapPass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/EquirectangularToCubemapBindGroupRG.generated.h"

namespace
{
using PassData = Passes::EquirectangularToCubemap::PassData;

PassData& convertEquirectangularToCubemapPass(StringId name, RG::Graph& renderGraph,
    RG::Resource equirectangular, RG::Resource cubemap, f32 exposure)
{
    using namespace RG;

    using PassDataBind = PassDataWithBind<PassData, EquirectangularToCubemapBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("EquirectangularToCubemap.Setup")

            passData.BindGroup = EquirectangularToCubemapBindGroupRG(graph);

            passData.BindGroup.SetResourcesEquirectangular(equirectangular);
            passData.Cubemap = passData.BindGroup.SetResourcesCubemap(cubemap);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("EquirectangularToCubemap")
            GPU_PROFILE_FRAME("EquirectangularToCubemap")

            auto& cubemapDescription = graph.GetImageDescription(cubemap);

            struct PushConstants
            {
                glm::vec2 CubemapResolutionInverse{};
                f32 Exposure{};
            };
            PushConstants pushConstants = {
                .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemapDescription.Width},
                .Exposure = exposure,
            };

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {pushConstants}
            });
            cmd.Dispatch({
                .Invocations = {cubemapDescription.Width, cubemapDescription.Width, 6},
                .GroupSize = passData.BindGroup.GetComputeMainGroupSize()
            });
        });
}
}

Passes::EquirectangularToCubemap::PassData& Passes::EquirectangularToCubemap::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    
    return renderGraph.AddRenderPass<PassData>(name,
       [&](Graph& graph, PassData& passData)
       {
           auto& convert = convertEquirectangularToCubemapPass(name.Concatenate(".Convert"), graph,
               info.Equirectangular, info.Cubemap, info.Exposure);

           auto& mipmap = Mipmap::addToGraph(name.Concatenate(".Mipmap"), graph, convert.Cubemap);
           passData.Cubemap = mipmap.Texture;
       },
       [=](const PassData&, FrameContext&, const Graph&)
       {
       });
}
