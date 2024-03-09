#include "HiZPass.h"

#include "Renderer.h"
#include "RenderGraph/RenderGraph.h"
#include "utils/MathUtils.h"
#include "Vulkan/RenderCommand.h"

HiZPass::HiZPass(RenderGraph::Graph& renderGraph, RenderGraph::Resource depth)
{
    CreateHiZ(RenderGraph::Resources(renderGraph).GetTexture(depth));

    AddSubPasses(renderGraph, depth);
}

HiZPass::~HiZPass()
{
    Image::Destroy(m_HiZ);
}

void HiZPass::CreateHiZ(const Texture& depth)
{
    u32 width = utils::floorToPowerOf2(depth.GetDescription().Width);
    u32 height = utils::floorToPowerOf2(depth.GetDescription().Height);
    
    Texture::Builder hizBuilder = Texture::Builder()
        .SetExtent({width, height})
        .SetFormat(Format::R32_FLOAT)
        .SetUsage(ImageUsage::Sampled | ImageUsage::Storage)
        .CreateMipmaps(true, ImageFilter::Linear);
    u32 mipmapCount = Image::CalculateMipmapCount({width, height});

    for (u32 i = 0; i < mipmapCount; i++)
        hizBuilder.AddView(
            {.MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1}, m_MipmapViewHandles[i]);

    m_HiZ = hizBuilder.BuildManualLifetime();

    m_MinMaxSampler = Sampler::Builder()
        .Filters(ImageFilter::Linear, ImageFilter::Linear)
        .MaxLod((f32)MAX_MIPMAP_COUNT)
        .WithAnisotropy(false)
        .ReductionMode(SamplerReductionMode::Min)
        .Build();
}

void HiZPass::AddSubPasses(RenderGraph::Graph& renderGraph,  RenderGraph::Resource depth)
{
    using namespace RenderGraph;
    
    u32 mipMapCount = m_HiZ.GetDescription().Mipmaps;
    u32 width = m_HiZ.GetDescription().Width;  
    u32 height = m_HiZ.GetDescription().Height;  
    
    ShaderPipelineTemplate* hizPassTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/render-graph/hiz-comp.shader"},
        "render-graph-hiz-pass-template", renderGraph.GetArenaAllocators());
    
    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(hizPassTemplate)
        .UseDescriptorBuffer()
        .Build();

    for (u32 i = 0; i < mipMapCount; i++)
    {
        ShaderDescriptors samplerDescriptors = ShaderDescriptors::Builder()
           .SetTemplate(hizPassTemplate, DescriptorAllocatorKind::Samplers)
           .ExtractSet(0)
           .Build();

        ShaderDescriptors resourceDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(hizPassTemplate, DescriptorAllocatorKind::Resources)
            .ExtractSet(1)
            .Build();
        
        ShaderDescriptors::BindingInfo samplerBinding = samplerDescriptors.GetBindingInfo("u_in_sampler");
        ShaderDescriptors::BindingInfo inImageBinding = resourceDescriptors.GetBindingInfo("u_in_image");
        ShaderDescriptors::BindingInfo outImageBinding = resourceDescriptors.GetBindingInfo("u_out_image");
 
        m_Passes[i] = &renderGraph.AddRenderPass<PassData>(std::format("hiz-subpass{}", i),
            [&](Graph& graph, PassData& passData)
            {
                Resource depthIn = {};
                Resource depthOut = {};
         
                if (i == 0)
                {
                    depthIn = depth;
                    depthOut = graph.AddExternal("hiz-out", m_HiZ);
                    passData.HiZMain = depthOut;
                }
                else
                {
                    PassData& previousOutput = graph.GetBlackboard().GetOutput<PassData>();
                    depthIn = previousOutput.HiZOut;
                    depthOut = previousOutput.HiZOut;
                    passData.HiZMain = previousOutput.HiZMain;
                }
                passData.DepthIn = graph.Read(depthIn,
                    ResourceAccessFlags::Compute | ResourceAccessFlags::Sampled);
                passData.HiZOut = graph.Write(depthOut,
                    ResourceAccessFlags::Compute | ResourceAccessFlags::Storage);

                passData.MinMaxSampler = m_MinMaxSampler;
                passData.MipmapViewHandles = m_MipmapViewHandles;
                
                graph.GetBlackboard().UpdateOutput(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                GPU_PROFILE_FRAME("HiZ")
                const Texture& depthIn = resources.GetTexture(passData.DepthIn);
                const Texture& hizOut = resources.GetTexture(passData.HiZOut);

                TextureBindingInfo depthInBinding = i > 0 ?
                    depthIn.CreateBindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[i - 1]) :
                    depthIn.CreateBindingInfo(passData.MinMaxSampler, ImageLayout::DepthReadonly);
                samplerDescriptors.UpdateBinding(samplerBinding, depthInBinding);
                resourceDescriptors.UpdateBinding(inImageBinding, depthInBinding);
                resourceDescriptors.UpdateBinding(outImageBinding,
                    hizOut.CreateBindingInfo(
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
}
