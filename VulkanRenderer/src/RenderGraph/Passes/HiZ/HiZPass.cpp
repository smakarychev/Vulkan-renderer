#include "HiZPass.h"

#include "Renderer.h"
#include "RenderGraph/RenderGraph.h"
#include "utils/MathUtils.h"
#include "Vulkan/RenderCommand.h"

HiZPassContext::HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue)
    : m_DrawResolution(resolution)
{
    u32 width = utils::floorToPowerOf2(resolution.x);
    u32 height = utils::floorToPowerOf2(resolution.y);
    
    u32 mipmapCount = Image::CalculateMipmapCount({width, height});

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

    m_MipmapViewHandles = m_HiZ.GetViewHandles();

    m_MinMaxSampler = Sampler::Builder()
        .Filters(ImageFilter::Linear, ImageFilter::Linear)
        .MaxLod((f32)MAX_MIPMAP_COUNT)
        .WithAnisotropy(false)
        .ReductionMode(SamplerReductionMode::Min)
        .Build();
}

HiZPass::HiZPass(RG::Graph& renderGraph, std::string_view baseName)
    : m_Name(baseName)
{
    ShaderPipelineTemplate* hizPassTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
       {"../assets/shaders/processed/render-graph/culling/hiz-comp.shader"},
       "Pass.HiZ", renderGraph.GetArenaAllocators());

    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(hizPassTemplate)
        .UseDescriptorBuffer()
        .Build();

    for (u32 i = 0; i < HiZPassContext::MAX_MIPMAP_COUNT; i++)
    {
        m_PipelinesData[i].Pipeline = pipeline;
        m_PipelinesData[i].SamplerDescriptors = ShaderDescriptors::Builder()
           .SetTemplate(hizPassTemplate, DescriptorAllocatorKind::Samplers)
           .ExtractSet(0)
           .Build();
        m_PipelinesData[i].ResourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(hizPassTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();
    }
}

void HiZPass::AddToGraph(RG::Graph& renderGraph, RG::Resource depth, HiZPassContext& ctx)
{
    using namespace RG;

    u32 mipMapCount = ctx.GetHiZ().GetDescription().Mipmaps;
    u32 width = ctx.GetHiZ().GetDescription().Width;  
    u32 height = ctx.GetHiZ().GetDescription().Height;
    
    static ShaderDescriptors::BindingInfo samplerBinding =
        m_PipelinesData[0].SamplerDescriptors.GetBindingInfo("u_in_sampler");
    static ShaderDescriptors::BindingInfo inImageBinding =
        m_PipelinesData[0].ResourceDescriptors.GetBindingInfo("u_in_image");
    static ShaderDescriptors::BindingInfo outImageBinding =
        m_PipelinesData[0].ResourceDescriptors.GetBindingInfo("u_out_image");

    for (u32 i = 0; i < mipMapCount; i++)
        m_Passes[i] = &renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.{}", m_Name.Name(), i)},
            [&](Graph& graph, PassData& passData)
            {
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
                    PassData& previousOutput = graph.GetBlackboard().Get<PassData>();
                    depthIn = previousOutput.HiZOut;
                    depthOut = previousOutput.HiZOut;
                }
                passData.DepthIn = graph.Read(depthIn,
                    ResourceAccessFlags::Compute | ResourceAccessFlags::Sampled);
                passData.HiZOut = graph.Write(depthOut,
                    ResourceAccessFlags::Compute | ResourceAccessFlags::Storage);

                passData.MinMaxSampler = ctx.GetSampler();
                passData.MipmapViewHandles = ctx.GetViewHandles();

                passData.PipelineData = &m_PipelinesData[i];
                
                graph.GetBlackboard().Update(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                GPU_PROFILE_FRAME("HiZ")
                const Texture& depthIn = resources.GetTexture(passData.DepthIn);
                const Texture& hizOut = resources.GetTexture(passData.HiZOut);

                TextureBindingInfo depthInBinding = i > 0 ?
                    depthIn.BindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[i - 1]) :
                    depthIn.BindingInfo(passData.MinMaxSampler, ImageLayout::DepthReadonly);

                auto& pipeline = passData.PipelineData->Pipeline;
                auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
                auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
                
                samplerDescriptors.UpdateBinding(samplerBinding, depthInBinding);
                resourceDescriptors.UpdateBinding(inImageBinding, depthInBinding);
                resourceDescriptors.UpdateBinding(outImageBinding,
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
}