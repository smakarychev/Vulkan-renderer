#include "rendererpch.h"

#include "EnvironmentPrefilterPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/EnvironmentPrefilterBindGroupRG.generated.h"
#include "Rendering/Image/ImageUtility.h"

Passes::EnvironmentPrefilter::PassData& Passes::EnvironmentPrefilter::addToGraph(StringId name, RG::Graph& renderGraph,
    Texture cubemap, Texture prefiltered, bool realTime)
{
    return addToGraph(name, renderGraph,
        renderGraph.Import("Cubemap"_hsv, cubemap, ImageLayout::Readonly),
        prefiltered, realTime);
}

Passes::EnvironmentPrefilter::PassData& Passes::EnvironmentPrefilter::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, Texture prefiltered, bool realTime)
{
    static constexpr i8 MAX_MIPMAP_COUNT = 16;
    
    using namespace RG;
    using enum ResourceAccessFlags;
    using PassDataBind = PassDataWithBind<PassData, EnvironmentPrefilterBindGroupRG>;


    i8 mipmapCount = Device::GetImageDescription(prefiltered).Mipmaps;
    Resource prefilteredResource = renderGraph.Import("EnvironmentPrefilter"_hsv, prefiltered);
    ASSERT(MAX_MIPMAP_COUNT > mipmapCount)
    std::array<Resource, MAX_MIPMAP_COUNT> mips = {};
    for (i8 i = 0; i < mipmapCount; i++)
        mips[i] = renderGraph.SplitImage(prefilteredResource, Device::GetAdditionalImageViews(prefiltered)[i]);
    
    for (i8 mipmap = 0; mipmap < mipmapCount; mipmap++)
    {
        PassDataBind& data = renderGraph.AddRenderPass<PassDataBind>(name.AddVersion(mipmap),
            [&](Graph& graph, PassDataBind& passData)
            {
                CPU_PROFILE_FRAME("EnvironmentPrefilter.Setup")

                passData.BindGroup = EnvironmentPrefilterBindGroupRG(graph, graph.SetShader("environmentPrefilter"_hsv,
                    ShaderSpecializations(ShaderSpecialization{"REAL_TIME"_hsv, realTime})));

                passData.PrefilteredTexture = passData.BindGroup.SetResourcesPrefiltered(mips[mipmap]);
                passData.Cubemap = passData.BindGroup.SetResourcesEnvironment(cubemap);
            },
            [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("EnvironmentPrefilter")
                GPU_PROFILE_FRAME("EnvironmentPrefilter")

                const ImageDescription& cubemapDescription = graph.GetImageDescription(passData.Cubemap);
                const ImageDescription& prefilteredDescription = graph.GetImageDescription(passData.PrefilteredTexture);

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
                passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                	.Data = {pushConstants}});
                cmd.Dispatch({
                    .Invocations = {resolution, resolution, 6},
                    .GroupSize = passData.BindGroup.GetComputeMainGroupSize()});
            });

        if (mipmap == mipmapCount - 1)
        {
            data.PrefilteredTexture = renderGraph.MergeImage(Span<const Resource>(mips.data(), mipmapCount));
            return data;
        }
    }

    std::unreachable();
}

TextureDescription Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(u32 resolution)
{
    i8 mipmapCount = Images::mipmapCount({resolution, resolution});
    std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
    for (i8 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription{
            .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 6};

    return {
        .Width = resolution,
        .Height = resolution,
        .LayersDepth = 6,
        .Mipmaps = mipmapCount,
        .Format = Format::RGBA16_FLOAT,
        .Kind = ImageKind::Cubemap,
        .Usage = ImageUsage::Sampled | ImageUsage::Storage,
        .AdditionalViews = additionalViews
    };
}
