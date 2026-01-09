#include "rendererpch.h"

#include "EquirectangularToCubemapPass.h"

#include "MipMapPass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/EquirectangularToCubemapBindGroupRG.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace
{
using PassData = Passes::EquirectangularToCubemap::PassData;

PassData& convertEquirectangularToCubemapPass(StringId name, RG::Graph& renderGraph,
    RG::Resource equirectangular, Texture cubemap)
{
    using namespace RG;

    using PassDataBind = PassDataWithBind<PassData, EquirectangularToCubemapBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("EquirectangularToCubemap.Setup")

            passData.BindGroup = EquirectangularToCubemapBindGroupRG(graph);

            passData.Cubemap = passData.BindGroup.SetResourcesCubemap(graph.Import("Cubemap"_hsv, cubemap));
            passData.Equirectangular = passData.BindGroup.SetResourcesEquirectangular(equirectangular);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("EquirectangularToCubemap")
            GPU_PROFILE_FRAME("EquirectangularToCubemap")

            auto& cubemapDescription = graph.GetImageDescription(passData.Cubemap);

            struct PushConstants
            {
                glm::vec2 CubemapResolutionInverse{};
            };
            PushConstants pushConstants = {
                .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemapDescription.Width}
            };

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
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
    RG::Graph& renderGraph, Texture equirectangular, Texture cubemap)
{
    return addToGraph(name, renderGraph,
        renderGraph.Import("Equirectangular"_hsv, equirectangular, ImageLayout::Readonly),
        cubemap);
}

Passes::EquirectangularToCubemap::PassData& Passes::EquirectangularToCubemap::addToGraph(StringId name,
    RG::Graph& renderGraph, RG::Resource equirectangular, Texture cubemap)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& convert = convertEquirectangularToCubemapPass(name.Concatenate(".Convert"), graph,
                equirectangular, cubemap);

            auto& mipmap = Mipmap::addToGraph(name.Concatenate(".Mipmap"), graph, convert.Cubemap);
            passData.Equirectangular = convert.Equirectangular;
            passData.Cubemap = mipmap.Texture;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
