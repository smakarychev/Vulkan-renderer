#include "EnvironmentPrefilterPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/EnvironmentPrefilterBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::EnvironmentPrefilter::addToGraph(StringId name, RG::Graph& renderGraph,
    Texture cubemap, Texture prefiltered)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal("Cubemap"_hsv, cubemap),
        prefiltered);
}

RG::Pass& Passes::EnvironmentPrefilter::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
    Texture prefiltered)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Resource prefilteredResource = {};
    i8 mipmaps = Device::GetImageDescription(prefiltered).Mipmaps;
    for (i8 mipmap = 0; mipmap < mipmaps; mipmap++)
    {
        Pass& pass = renderGraph.AddRenderPass<PassData>(name.AddVersion(mipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("EnvironmentPrefilter.Setup")

                graph.SetShader("environment-prefilter"_hsv);

                if (mipmap == 0)
                {
                    passData.PrefilteredTexture = graph.AddExternal(
                        "EnvironmentPrefilter"_hsv, prefiltered);
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

                auto&& [cubemapTexture, cubemapDescription] = resources.GetTextureWithDescription(passData.Cubemap);
                auto&& [prefilteredTexture, prefilteredDescription] =
                    resources.GetTextureWithDescription(passData.PrefilteredTexture);

                const Shader& shader = resources.GetGraph()->GetShader();
                EnvironmentPrefilterShaderBindGroup bindGroup(shader);

                bindGroup.SetEnv({.Image = cubemapTexture}, ImageLayout::Readonly);
                bindGroup.SetPrefilter({
                    .Image = prefilteredTexture,
                    .Description = Device::GetAdditionalImageViews(prefilteredTexture)[mipmap]},
                    ImageLayout::General);

                u32 resolution = std::max(1u, prefilteredDescription.Width >> (u32)mipmap);

                struct PushConstants
                {
                    glm::vec2 PrefilterResolutionInverse{};
                    glm::vec2 EnvironmentResolutionInverse{};
                    f32 Roughness{};
                };
                PushConstants pushConstants = {
                    .PrefilterResolutionInverse = 1.0f / glm::vec2{(f32)resolution},
                    .EnvironmentResolutionInverse = 1.0f / glm::vec2{
                        (f32)cubemapDescription.Width, (f32)cubemapDescription.Height},
                    .Roughness = (f32)mipmap / (f32)prefilteredDescription.Mipmaps};

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
                cmd.PushConstants({
                	.PipelineLayout = shader.GetLayout(), 
                	.Data = {pushConstants}});
                cmd.Dispatch({
                    .Invocations = {resolution, resolution, 6},
                    .GroupSize = {32, 32, 1}});
            });

        if (mipmap == mipmaps - 1)
            return pass;
    }

    std::unreachable();
}

TextureDescription Passes::EnvironmentPrefilter::getPrefilteredTextureDescription()
{
    i8 mipmapCount = Images::mipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION});
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
