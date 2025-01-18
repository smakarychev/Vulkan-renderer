#include "EnvironmentPrefilterPass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::EnvironmentPrefilter::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const Texture& cubemap, const Texture& prefiltered)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.Cubemap", name), cubemap),
        prefiltered);
}

RG::Pass& Passes::EnvironmentPrefilter::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
    const Texture& prefiltered)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Resource prefilteredResource = {};
    for (i8 mipmap = 0; mipmap < prefiltered.Description().Mipmaps; mipmap++)
    {
        Pass& pass = renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.{}", name, mipmap)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("EnvironmentPrefilter.Setup")

                graph.SetShader("../assets/shaders/environment-prefilter.shader");

                if (mipmap == 0)
                {
                    passData.PrefilteredTexture = graph.AddExternal(
                        std::format("{}.EnvironmentPrefilter", name), prefiltered);
                    prefilteredResource = passData.PrefilteredTexture;
                }
                    
                passData.PrefilteredTexture = graph.Write(prefilteredResource, Compute | Storage);
                prefilteredResource = passData.PrefilteredTexture;
                passData.Cubemap = graph.Read(cubemap, Compute | Sampled);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("EnvironmentPrefilter")
                GPU_PROFILE_FRAME("EnvironmentPrefilter")

                const Texture& cubemapTexture = resources.GetTexture(passData.Cubemap);
                const Texture& prefilteredTexture = resources.GetTexture(passData.PrefilteredTexture);

                const Shader& shader = resources.GetGraph()->GetShader();
                auto pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                resourceDescriptors.UpdateBinding("u_env", cubemapTexture.BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
                resourceDescriptors.UpdateBinding("u_prefilter", prefilteredTexture.BindingInfo(
                    ImageFilter::Linear, ImageLayout::General, prefilteredTexture.GetAdditionalViewHandles()[mipmap]));

                u32 resolution = std::max(1u, prefiltered.Description().Width >> (u32)mipmap);

                struct PushConstants
                {
                    glm::vec2 PrefilterResolutionInverse{};
                    glm::vec2 EnvironmentResolutionInverse{};
                    f32 Roughness{};
                };
                PushConstants pushConstants = {
                    .PrefilterResolutionInverse = 1.0f / glm::vec2{(f32)resolution},
                    .EnvironmentResolutionInverse = 1.0f / glm::vec2{
                        (f32)cubemapTexture.Description().Width, (f32)cubemapTexture.Description().Height},
                    .Roughness = (f32)mipmap / (f32)prefilteredTexture.Description().Mipmaps};

                auto& cmd = frameContext.Cmd;
                samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
                RenderCommand::BindCompute(cmd, pipeline);
                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstants);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

                RenderCommand::Dispatch(cmd,
                    {resolution, resolution, 6},
                    {32, 32, 1});
            });

        if (mipmap == prefiltered.Description().Mipmaps - 1)
            return pass;
    }

    std::unreachable();
}

TextureDescription Passes::EnvironmentPrefilter::getPrefilteredTextureDescription()
{
    i8 mipmapCount = Texture::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION});
    std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
    for (i8 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription{
            .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 6};

    return {
        .Width = PREFILTER_RESOLUTION,
        .Height = PREFILTER_RESOLUTION,
        .LayersDepth = 6,
        .Mipmaps = mipmapCount,
        .Format = Format::RGBA16_FLOAT,
        .Kind = ImageKind::Cubemap,
        .Usage = ImageUsage::Sampled | ImageUsage::Storage,
        .AdditionalViews = additionalViews};
}
