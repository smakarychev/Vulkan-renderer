#include "EquirectangularToCubemapPass.h"

#include "MipMapPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/EquirectangularToCubemapBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    struct ConvertPassData : Passes::EquirectangularToCubemap::PassData
    {};
    
    RG::Pass& convertEquirectangularToCubemapPass(std::string_view name, RG::Graph& renderGraph,
        RG::Resource equirectangular, const Texture& cubemap)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<ConvertPassData>(name,
            [&](Graph& graph, ConvertPassData& passData)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap.Setup")

                graph.SetShader("equirectangular-to-cubemap.shader");
                
                passData.Cubemap = graph.AddExternal(std::format("{}.Cubemap", name), cubemap);
                
                passData.Cubemap = graph.Write(passData.Cubemap, Compute | Storage);
                passData.Equirectangular = graph.Read(equirectangular, Compute | Sampled);
                
                graph.UpdateBlackboard(passData);
            },
            [=](ConvertPassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("EquirectangularToCubemap")
                GPU_PROFILE_FRAME("EquirectangularToCubemap")

                const Texture& equirectangularTexture = resources.GetTexture(passData.Equirectangular);
                const Texture& cubemapTexture = resources.GetTexture(passData.Cubemap);

                const Shader& shader = resources.GetGraph()->GetShader();
                EquirectangularToCubemapShaderBindGroup bindGroup(shader);

                bindGroup.SetEquirectangular(equirectangularTexture.BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
                bindGroup.SetCubemap(cubemapTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

                struct PushConstants
                {
                    glm::vec2 CubemapResolutionInverse{};
                };
                PushConstants pushConstants = {
                    .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemapTexture.Description().Width}};
                
                auto& cmd = frameContext.Cmd;
                bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstants);
                RenderCommand::Dispatch(cmd,
                    {cubemap.Description().Width, cubemap.Description().Width, 6},
                    {32, 32, 1});
            });
    }
}

RG::Pass& Passes::EquirectangularToCubemap::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const Texture& equirectangular, const Texture& cubemap)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.Equirectangular", name), equirectangular),
        cubemap);
}

RG::Pass& Passes::EquirectangularToCubemap::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource equirectangular, const Texture& cubemap)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& convert = convertEquirectangularToCubemapPass(std::format("{}.Convert", name), graph,
                equirectangular, cubemap);
            auto& convertOutput = graph.GetBlackboard().Get<ConvertPassData>(convert);
            
            auto& mipmap = Mipmap::addToGraph(std::format("{}.Mipmap", name), graph, convertOutput.Cubemap);
            auto& mipmapOutput = graph.GetBlackboard().Get<Mipmap::PassData>(mipmap);
            passData.Equirectangular = convertOutput.Equirectangular;
            passData.Cubemap = mipmapOutput.Texture;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
