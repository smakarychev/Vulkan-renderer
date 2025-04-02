#include "EquirectangularToCubemapPass.h"

#include "MipMapPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/EquirectangularToCubemapBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace
{
    struct ConvertPassData : Passes::EquirectangularToCubemap::PassData
    {};
    
    RG::Pass& convertEquirectangularToCubemapPass(StringId name, RG::Graph& renderGraph,
        RG::Resource equirectangular, Texture cubemap)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<ConvertPassData>(name,
            [&](Graph& graph, ConvertPassData& passData)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap.Setup")

                graph.SetShader("equirectangular-to-cubemap.shader");
                
                passData.Cubemap = graph.AddExternal("Cubemap"_hsv, cubemap);
                
                passData.Cubemap = graph.Write(passData.Cubemap, Compute | Storage);
                passData.Equirectangular = graph.Read(equirectangular, Compute | Sampled);
                
                graph.UpdateBlackboard(passData);
            },
            [=](ConvertPassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap")
                GPU_PROFILE_FRAME("EquirectangularToCubemap")

                Texture equirectangularTexture = resources.GetTexture(passData.Equirectangular);
                auto&& [cubemapTexture, cubemapDescription] = resources.GetTextureWithDescription(passData.Cubemap);

                const Shader& shader = resources.GetGraph()->GetShader();
                EquirectangularToCubemapShaderBindGroup bindGroup(shader);

                bindGroup.SetEquirectangular({.Image = equirectangularTexture}, ImageLayout::Readonly);
                bindGroup.SetCubemap({.Image = cubemapTexture}, ImageLayout::General);

                struct PushConstants
                {
                    glm::vec2 CubemapResolutionInverse{};
                };
                PushConstants pushConstants = {
                    .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemapDescription.Width}};
                
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                cmd.PushConstants({
                	.PipelineLayout = shader.GetLayout(), 
                	.Data = {pushConstants}});
                cmd.Dispatch({
                    .Invocations = {cubemapDescription.Width, cubemapDescription.Width, 6},
                    .GroupSize = {32, 32, 1}});
            });
    }
}

RG::Pass& Passes::EquirectangularToCubemap::addToGraph(StringId name, RG::Graph& renderGraph,
    Texture equirectangular, Texture cubemap)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal("Equirectangular"_hsv, equirectangular),
        cubemap);
}

RG::Pass& Passes::EquirectangularToCubemap::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource equirectangular, Texture cubemap)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& convert = convertEquirectangularToCubemapPass(name.Concatenate(".Convert"), graph,
                equirectangular, cubemap);
            auto& convertOutput = graph.GetBlackboard().Get<ConvertPassData>(convert);
            
            auto& mipmap = Mipmap::addToGraph(name.Concatenate(".Mipmap"), graph, convertOutput.Cubemap);
            auto& mipmapOutput = graph.GetBlackboard().Get<Mipmap::PassData>(mipmap);
            passData.Equirectangular = convertOutput.Equirectangular;
            passData.Cubemap = mipmapOutput.Texture;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
