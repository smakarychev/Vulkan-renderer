#include "rendererpch.h"

#include "TextureUtilityPasses.h"

#include "ChannelCompositionInfo.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/Texture2dToTexture2dBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/Texture3dToSliceBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/TextureArrayToAtlasBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/TextureArrayToSliceBindGroupRG.generated.h"

RG::Resource Passes::Texture2dToTexture2d::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition)
{
    using namespace RG;
    struct PassData
    {
        Resource Texture{};
        Resource TextureOut{};
        Texture2dToTexture2dBindGroupRG BindGroup;
    };
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Texture2dToTexture2d.Setup")

            passData.BindGroup = Texture2dToTexture2dBindGroupRG(graph,
                ShaderDefines({
                    ShaderDefine{"R_CHANNEL"_hsv, (u32)channelComposition.R},
                    ShaderDefine{"G_CHANNEL"_hsv, (u32)channelComposition.G},
                    ShaderDefine{"B_CHANNEL"_hsv, (u32)channelComposition.B},
                    ShaderDefine{"A_CHANNEL"_hsv, (u32)channelComposition.A}
                }));

            passData.TextureOut = graph.Create("Out"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = textureIn,
                .Format = Texture2dToTexture2dBindGroupRG::GetColorAttachmentFormat()
            });

            passData.Texture = passData.BindGroup.SetResourcesTexture(textureIn);
            passData.TextureOut = graph.RenderTarget(passData.TextureOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Texture2dToTexture2d")
            GPU_PROFILE_FRAME("Texture2dToTexture2d")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.Draw({.VertexCount = 3});
        }).TextureOut;
}


RG::Resource Passes::Texture3dToSlice::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    f32 sliceNormalized, ChannelComposition channelComposition)
{
    using namespace RG;
    struct PassData
    {
        Resource Slice{};
        Resource Texture3d{};
        Texture3dToSliceBindGroupRG BindGroup;
    };
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Texture3dToSlice.Setup")

            passData.BindGroup = Texture3dToSliceBindGroupRG(graph,
                ShaderDefines({
                    ShaderDefine{"R_CHANNEL"_hsv, (u32)channelComposition.R},
                    ShaderDefine{"G_CHANNEL"_hsv, (u32)channelComposition.G},
                    ShaderDefine{"B_CHANNEL"_hsv, (u32)channelComposition.B},
                    ShaderDefine{"A_CHANNEL"_hsv, (u32)channelComposition.A}
                }));

            passData.Slice = graph.Create("Slice"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = textureIn,
                .Format = Texture3dToSliceBindGroupRG::GetColorAttachmentFormat()
            });

            passData.Texture3d = passData.BindGroup.SetResourcesTexture(textureIn);
            passData.Slice = graph.RenderTarget(passData.Slice, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Texture3dToSlice")
            GPU_PROFILE_FRAME("Texture3dToSlice")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {sliceNormalized}});
            cmd.Draw({.VertexCount = 3});
        }).Slice;
}

RG::Resource Passes::TextureArrayToSlice::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    u32 slice, ChannelComposition channelComposition)
{
    using namespace RG;
    struct PassData
    {
        Resource Slice{};
        Resource TextureArray{};
        TextureArrayToSliceBindGroupRG BindGroup;
    };

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("TextureArrayToSlice.Setup")

            passData.BindGroup = TextureArrayToSliceBindGroupRG(graph,
                ShaderDefines({
                    ShaderDefine{"R_CHANNEL"_hsv, (u32)channelComposition.R},
                    ShaderDefine{"G_CHANNEL"_hsv, (u32)channelComposition.G},
                    ShaderDefine{"B_CHANNEL"_hsv, (u32)channelComposition.B},
                    ShaderDefine{"A_CHANNEL"_hsv, (u32)channelComposition.A}
                }));
            
            passData.Slice = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = textureIn,
                .Format = TextureArrayToSliceBindGroupRG::GetColorAttachmentFormat()
            });

            passData.TextureArray = passData.BindGroup.SetResourcesTextureArray(textureIn);
            passData.Slice = graph.RenderTarget(passData.Slice, {
                .OnLoad = AttachmentLoad::Clear,
                .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}
            });
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("TextureArrayToSlice")
            GPU_PROFILE_FRAME("TextureArrayToSlice")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.PushConstants({
            	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
            	.Data = {slice}});
            cmd.Draw({.VertexCount = 3});
        }).Slice;
}

RG::Resource Passes::TextureArrayToAtlas::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition)
{
    using namespace RG;
    struct PassData
    {
        Resource Atlas{};
        Resource TextureArray{};
        TextureArrayToAtlasBindGroupRG BindGroup;
    };

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("TextureArrayToAtlas.Setup")

            passData.BindGroup = TextureArrayToAtlasBindGroupRG(graph,
                ShaderDefines({
                    ShaderDefine{"R_CHANNEL"_hsv, (u32)channelComposition.R},
                    ShaderDefine{"G_CHANNEL"_hsv, (u32)channelComposition.G},
                    ShaderDefine{"B_CHANNEL"_hsv, (u32)channelComposition.B},
                    ShaderDefine{"A_CHANNEL"_hsv, (u32)channelComposition.A}}));

            passData.Atlas = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = textureIn,
                .Format = TextureArrayToAtlasBindGroupRG::GetColorAttachmentFormat()
            });

            passData.TextureArray = passData.BindGroup.SetResourcesTextureArray(textureIn);
            passData.Atlas = graph.RenderTarget(passData.Atlas, {
                .OnLoad = AttachmentLoad::Clear,
                .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}
            });
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("TextureArrayToAtlas")
            GPU_PROFILE_FRAME("TextureArrayToAtlas")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);

            const ImageDescription& description = graph.GetImageDescription(passData.TextureArray);
            const f32 squareDimensions = (f32)std::floor(std::sqrt(description.LayersDepth));
            f32 width = squareDimensions;
            f32 height = squareDimensions;
            if (squareDimensions * squareDimensions < (f32)description.LayersDepth)
                width += 1;
            
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {width, height}});
            cmd.Draw({.VertexCount = 3});
        }).Atlas;
}