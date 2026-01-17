#include "rendererpch.h"

#include "CloudShapeNoisePass.h"

#include "RenderGraph/Passes/Generated/CloudsMapShapeBindGroupRG.generated.h"
#include "RenderGraph/Passes/Utility/MipMapPass.h"
#include "Rendering/Image/ImageUtility.h"

namespace
{
RG::Resource& createShapeTexture(StringId name, RG::Graph& renderGraph, f32 textureSize, bool isHighFrequency,
    const RG::Resource noiseParameters, Image external)
{
    using namespace RG;
    struct CreateShapePassData
    {
        Resource Shape{};
    };
    using PassDataBind = PassDataWithBind<CreateShapePassData, CloudsMapShapeBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Cloud.ShapeTexture.Setup")

            passData.BindGroup = CloudsMapShapeBindGroupRG(graph, ShaderSpecializations{
                ShaderSpecialization{"IS_HIGH_FREQUENCY"_hsv, isHighFrequency}
            });

            if (external.HasValue())
                passData.Shape = graph.Import("ShapeTexture.Imported"_hsv, external);
            else
                passData.Shape = graph.Create("ShapeTexture"_hsv, RGImageDescription{
                    .Width = textureSize,
                    .Height = textureSize,
                    .LayersDepth = textureSize,
                    .Mipmaps = Images::mipmapCount(glm::uvec2((u32)textureSize)),
                    .Format = Format::R16_FLOAT,
                    .Kind = ImageKind::Image3d,
                });

            passData.Shape = passData.BindGroup.SetResourcesShape(passData.Shape);
            passData.BindGroup.SetResourcesParameters(noiseParameters);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Cloud.ShapeTexture")
            GPU_PROFILE_FRAME("Cloud.ShapeTexture")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {textureSize, textureSize, textureSize},
                .GroupSize = passData.BindGroup.GetCloudShapeNoiseGroupSize()
            });
        }).Shape;
}
}

Passes::Clouds::ShapeNoise::PassData& Passes::Clouds::ShapeNoise::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Cloud.ShapeNoise.Setup")

            passData.LowFrequencyTexture = createShapeTexture("LowFrequency"_hsv, graph, info.LowFrequencyTextureSize,
                false, info.LowFrequencyNoiseParameters, info.LowFrequencyTexture);
            passData.HighFrequencyTexture = createShapeTexture("HighFrequency"_hsv, graph,
                info.HighFrequencyTextureSize, true, info.HighFrequencyNoiseParameters, info.HighFrequencyTexture);

            passData.LowFrequencyTexture =
                Mipmap::addToGraph("LowFrequency.Mipmap"_hsv, graph, passData.LowFrequencyTexture).Texture;
            passData.HighFrequencyTexture =
                Mipmap::addToGraph("HighFrequency.Mipmap"_hsv, graph, passData.HighFrequencyTexture).Texture;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
