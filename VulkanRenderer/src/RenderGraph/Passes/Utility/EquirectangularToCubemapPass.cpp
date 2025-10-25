#include "rendererpch.h"

#include "EquirectangularToCubemapPass.h"

#include "MipMapPass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/EquirectangularToCubemapBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace
{
    struct ConvertPassData : Passes::EquirectangularToCubemap::PassData
    {};
    
    ConvertPassData& convertEquirectangularToCubemapPass(StringId name, RG::Graph& renderGraph,
        RG::Resource equirectangular, Texture cubemap)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<ConvertPassData>(name,
            [&](Graph& graph, ConvertPassData& passData)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap.Setup")

                graph.SetShader("equirectangular-to-cubemap"_hsv);
                
                passData.Cubemap = graph.Import("Cubemap"_hsv, cubemap);
                
                passData.Cubemap = graph.WriteImage(passData.Cubemap, Compute | Storage);
                passData.Equirectangular = graph.ReadImage(equirectangular, Compute | Sampled);
            },
            [=](const ConvertPassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap")
                GPU_PROFILE_FRAME("EquirectangularToCubemap")

                auto& cubemapDescription = graph.GetImageDescription(passData.Cubemap);

                const Shader& shader = graph.GetShader();
                EquirectangularToCubemapShaderBindGroup bindGroup(shader);

                bindGroup.SetEquirectangular(graph.GetImageBinding(passData.Equirectangular));
                bindGroup.SetCubemap(graph.GetImageBinding(passData.Cubemap));

                struct PushConstants
                {
                    glm::vec2 CubemapResolutionInverse{};
                };
                PushConstants pushConstants = {
                    .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemapDescription.Width}};
                
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                	.PipelineLayout = shader.GetLayout(), 
                	.Data = {pushConstants}});
                cmd.Dispatch({
                    .Invocations = {cubemapDescription.Width, cubemapDescription.Width, 6},
                    .GroupSize = {32, 32, 1}});
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
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& convert = convertEquirectangularToCubemapPass(name.Concatenate(".Convert"), graph,
                equirectangular, cubemap);
            
            auto& mipmap = Mipmap::addToGraph(name.Concatenate(".Mipmap"), graph, convert.Cubemap);
            passData.Equirectangular = convert.Equirectangular;
            passData.Cubemap = mipmap.Texture;
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
        });
}
