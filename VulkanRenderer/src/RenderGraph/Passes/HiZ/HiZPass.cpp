#include "HiZPass.h"

#include "Renderer.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "utils/MathUtils.h"
#include "Vulkan/RenderCommand.h"

HiZPassContext::HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue)
    : m_DrawResolution(resolution)
{
    u32 width = MathUtils::floorToPowerOf2(resolution.x);
    u32 height = MathUtils::floorToPowerOf2(resolution.y);

    m_HiZResolution = glm::uvec2{width, height};
    
    u32 mipmapCount = std::min(MAX_MIPMAP_COUNT, Image::CalculateMipmapCount({width, height}));

    std::vector<ImageSubresourceDescription::Packed> additionalViews(mipmapCount);
    for (u32 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription::Pack({
            .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1});
    
    m_HiZ = Texture::Builder({
            .Width = width,
            .Height = height,
            .Mipmaps = mipmapCount,
            .Format = Format::R32_FLOAT,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage,
            .AdditionalViews = additionalViews})
        .Build(deletionQueue);

    m_MipmapViewHandles = m_HiZ.GetAdditionalViewHandles();

    m_MinMaxSampler = Sampler::Builder()
        .Filters(ImageFilter::Linear, ImageFilter::Linear)
        .MaxLod((f32)MAX_MIPMAP_COUNT)
        .WithAnisotropy(false)
        .ReductionMode(SamplerReductionMode::Min)
        .Build();
}

RG::Pass& Passes::HiZ::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    u32 mipMapCount = ctx.GetHiZ().Description().Mipmaps;
    u32 width = ctx.GetHiZ().Description().Width;  
    u32 height = ctx.GetHiZ().Description().Height;

    for (u32 i = 0; i < mipMapCount; i++)
    {
        Pass& pass = renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.{}", name, i)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZ.Setup")

                graph.SetShader("../assets/shaders/hiz.shader");
                
                Resource depthIn = {};
                Resource depthOut = {};
         
                if (i == 0)
                {
                    depthIn = depth;
                    depthOut = graph.AddExternal("Hiz.Out", ctx.GetHiZ());
                    graph.Export(depthOut, ctx.GetHiZPrevious(), true);
                }
                else
                {
                    depthIn = ctx.GetHiZResource(); 
                    depthOut = ctx.GetHiZResource();
                }
                passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
                passData.HiZOut = graph.Write(depthOut, Compute | Storage);

                passData.MinMaxSampler = ctx.GetSampler();
                passData.MipmapViewHandles = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZ")
                GPU_PROFILE_FRAME("HiZ")
                
                const Texture& depthIn = resources.GetTexture(passData.DepthIn);
                const Texture& hizOut = resources.GetTexture(passData.HiZOut);
                
                TextureBindingInfo depthInBinding = i > 0 ?
                    depthIn.BindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[i - 1]) :
                    depthIn.BindingInfo(passData.MinMaxSampler, ImageLayout::DepthReadonly,
                        depthIn.GetViewHandle(subresource));

                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
                
                samplerDescriptors.UpdateBinding("u_in_sampler", depthInBinding);
                resourceDescriptors.UpdateBinding("u_in_image", depthInBinding);
                resourceDescriptors.UpdateBinding("u_out_image",
                    hizOut.BindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[i]));
                
                u32 levelWidth = std::max(1u, width >> i);
                u32 levelHeight = std::max(1u, height >> i);
                glm::uvec2 levels = {levelWidth, levelHeight};

                pipeline.BindCompute(frameContext.Cmd);
                RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), levels);
                samplerDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                RenderCommand::Dispatch(frameContext.Cmd, {(levelWidth + 32 - 1) / 32, (levelHeight + 32 - 1) / 32, 1});
            });

        if (i == mipMapCount - 1)
            return pass;
    }

    std::unreachable();
}
