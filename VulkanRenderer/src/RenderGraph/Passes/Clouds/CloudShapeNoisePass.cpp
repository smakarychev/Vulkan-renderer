#include "rendererpch.h"

#include "CloudShapeNoisePass.h"

#include "CloudCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudShapeBindGroup.generated.h"
#include "RenderGraph/Passes/Utility/MipMapPass.h"
#include "Rendering/Image/ImageUtility.h"

namespace
{
    RG::Resource& createShapeTexture(StringId name, RG::Graph& renderGraph, f32 textureSize, bool isHighFrequency,
        const Passes::Clouds::CloudsNoiseParameters& noiseParameters, Image external)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<Resource>(name,
            [&](Graph& graph, Resource& shapeTexture)
            {
                CPU_PROFILE_FRAME("Cloud.ShapeTexture.Setup")

                graph.SetShader("cloud-shape"_hsv,
                    ShaderSpecializations{
                        ShaderSpecialization{"IS_HIGH_FREQUENCY"_hsv, isHighFrequency}
                    });

                if (external.HasValue())
                {
                    shapeTexture = graph.Import("ShapeTexture.Imported"_hsv, external);
                }
                else
                {
                    shapeTexture = graph.Create("ShapeTexture"_hsv, RGImageDescription{
                        .Width = textureSize,
                        .Height = textureSize,
                        .LayersDepth = textureSize,
                        .Mipmaps = Images::mipmapCount(glm::uvec2((u32)textureSize)),
                        .Format = Format::R16_FLOAT,
                        .Kind = ImageKind::Image3d,
                    });
                }
                
                shapeTexture = graph.WriteImage(shapeTexture, Compute | Storage);
            },
            [=](const Resource& shapeTexture, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("Cloud.ShapeTexture")
                GPU_PROFILE_FRAME("Cloud.ShapeTexture")

                const Shader& shader = graph.GetShader();
                CloudShapeShaderBindGroup bindGroup(shader);
                bindGroup.SetCloudShape(graph.GetImageBinding(shapeTexture));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(),
                    .Data = {noiseParameters}
                });
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {textureSize, textureSize, textureSize},
                    .GroupSize = {8, 8, 8}
                });
            });
    }
}

Passes::Clouds::ShapeNoise::PassData& Passes::Clouds::ShapeNoise::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Cloud.ShapeNoise.Setup")

            passData.LowFrequencyTexture = createShapeTexture("LowFrequency"_hsv, graph, info.LowFrequencyTextureSize,
                false, *info.LowFrequencyNoiseParameters, info.LowFrequencyTexture);
            passData.HighFrequencyTexture = createShapeTexture("HighFrequency"_hsv, graph,
                info.HighFrequencyTextureSize, true, *info.HighFrequencyNoiseParameters, info.HighFrequencyTexture);

            passData.LowFrequencyTexture =
                Mipmap::addToGraph("LowFrequency.Mipmap"_hsv, graph, passData.LowFrequencyTexture).Texture;
            passData.HighFrequencyTexture =
                Mipmap::addToGraph("HighFrequency.Mipmap"_hsv, graph, passData.HighFrequencyTexture).Texture;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
