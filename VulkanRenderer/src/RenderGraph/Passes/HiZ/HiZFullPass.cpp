#include "HiZFullPass.h"

#include "HiZBlitPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/DepthReductionBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::HiZFull::PassData& Passes::HiZFull::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;
    using enum ResourceAccessFlags;

    auto& minBlit = HiZBlit::addToGraph(name.Concatenate(".BlitMin"), renderGraph, {
        .Depth = info.Depth,
        .Subresource = info.Subresource,
        .ReductionMode = HiZ::ReductionMode::Min,
        .CalculateMinMax = true});
    auto& maxBlit = HiZBlit::addToGraph(name.Concatenate(".BlitMax"), renderGraph, {
        .Depth = info.Depth,
        .Subresource = info.Subresource,
        .ReductionMode = HiZ::ReductionMode::Max,
        .CalculateMinMax = false});

    const u32 mipmapCount = (u32)(u8)renderGraph.GetTextureDescription(minBlit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetTextureDescription(minBlit.HiZ).Dimensions();
    u32 width = hizResolution.x;  
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;
    
    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);
        PassData& data = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZFull.Setup")

                graph.SetShader("depth-reduction"_hsv);

                passData.Depth = graph.Read(minBlit.Depth, Compute | Sampled);
                minBlit.HiZ = graph.Read(minBlit.HiZ, Compute | Sampled);
                minBlit.HiZ= graph.Write(minBlit.HiZ, Compute | Storage);
                maxBlit.HiZ = graph.Read(maxBlit.HiZ, Compute | Sampled);
                maxBlit.HiZ = graph.Write(maxBlit.HiZ, Compute | Storage);
                passData.HiZMin = minBlit.HiZ;
                passData.HiZMax = maxBlit.HiZ;
                passData.MinMaxDepth = minBlit.MinMaxDepth;
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZFull")
                GPU_PROFILE_FRAME("HiZFull")

                Texture hizMin = resources.GetTexture(passData.HiZMin);
                Texture hizMax = resources.GetTexture(passData.HiZMax);
                Span hizViews = Device::GetAdditionalImageViews(hizMin);

                const Shader& shader = resources.GetGraph()->GetShader();
                DepthReductionShaderBindGroup bindGroup(shader);
                bindGroup.SetMinSampler(HiZ::createSampler(HiZ::ReductionMode::Min));
                bindGroup.SetMaxSampler(HiZ::createSampler(HiZ::ReductionMode::Max));
                bindGroup.SetMinImage({.Image = hizMin}, ImageLayout::General);
                bindGroup.SetMaxImage({.Image = hizMax}, ImageLayout::General);
                
                for (u32 i = 0; i < hizViews.size(); i++)
                {
                    bindGroup.SetOutputMin({
                        .Image = hizMin,
                        .Description = hizViews[i]}, ImageLayout::General, i);
                    bindGroup.SetOutputMax({
                        .Image = hizMax,
                        .Description = hizViews[i]}, ImageLayout::General, i);
                }

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                u32 shift = toBeProcessed > 5 ? 12 : 10;
                u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                u32 samples = width * height;
                cmd.Dispatch({
                    .Invocations = {(samples + mask) >> shift, 1, 1}});
            }).Data;

        width = std::max(1u, width >> toBeProcessed);
        height = std::max(1u, height >> toBeProcessed);
        currentMipmap += toBeProcessed;
        mipmapsRemaining -= toBeProcessed;

        if (mipmapsRemaining == 0)
            return data;
    }

    std::unreachable();
}