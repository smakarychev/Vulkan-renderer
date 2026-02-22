#include "rendererpch.h"

#include "ImGuiTexturePass.h"

#include "BlitPass.h"
#include "TextureUtilityPasses.h"
#include "imgui/imgui.h"
#include "Imgui/ImguiUI.h"
#include "RenderGraph/RGGraph.h"

namespace
{
glm::vec2 getTextureWindowSize(const TextureDescription& texture)
{
    const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    glm::vec2 size = {availableRegion.x, availableRegion.y};
    // save the aspect ratio of image
    const f32 aspect = texture.AspectRatio();
    if (aspect > 1.0f)
        size.y = size.x / aspect;
    else
        size.x = size.y * aspect;

    return size;
}

template <typename WidgetFn>
RG::Resource imgui2dTexturePass(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, WidgetFn&& widgetFn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Texture;
        StringId Name{};
    };
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            ImGui::Begin(name.AsString().c_str());
            const glm::vec2 size = glm::max(glm::vec2(1.0f),
                getTextureWindowSize(graph.GetImageDescription(textureIn)));
            passData.Texture = graph.Create("ImguiSource"_hsv, ResourceCreationFlags::Volatile, {
                .Width = size.x,
                .Height = size.y,
                .Format = Format::RGBA16_FLOAT,
            });
            ImGui::End();

            passData.Texture = Passes::Blit::addToGraph(
                "ImguiBlit"_hsv, graph, textureIn, passData.Texture, {}, 1.0, ImageFilter::Linear).TextureOut;
            passData.Texture = graph.ReadImage(passData.Texture, Pixel | Sampled);
            passData.Name = name;
            graph.HasSideEffect();
        },
        [=](const PassDataPrivate& passData, FrameContext&, const Graph& graph)
        {
            CPU_PROFILE_FRAME("ImGui Texture")
            GPU_PROFILE_FRAME("ImGui Texture")

            auto&& [texture, description] = graph.GetImageWithDescription(passData.Texture);

            ImGui::Begin(passData.Name.AsString().c_str());
            widgetFn();
            const glm::vec2 size = getTextureWindowSize(description);
            const Sampler sampler = Device::CreateSampler({.WrapMode = SamplerWrapMode::ClampEdge});
            ImGuiUI::Texture(ImageSubresource{.Image = texture}, sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        }).Texture;
}
}

RG::Resource Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture)
{
    return addToGraph(name, renderGraph, renderGraph.Import("In"_hsv, texture, ImageLayout::Readonly));
}

RG::Resource Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    return imgui2dTexturePass(name, renderGraph, textureIn, []()
    {
    });
}

RG::Resource Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture,
    ChannelComposition channelComposition)
{
    return addToGraph(name, renderGraph, renderGraph.Import("In"_hsv, texture, ImageLayout::Readonly),
        channelComposition);
}

RG::Resource Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition)
{
    return addToGraph(name, renderGraph, Texture2dToTexture2d::addToGraph(
        name.Concatenate(".TextureToTexture"), renderGraph, textureIn, channelComposition));
}

RG::Resource Passes::ImGuiCubeTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture)
{
    return addToGraph(name, renderGraph, renderGraph.Import("In"_hsv, texture, ImageLayout::Readonly));
}

RG::Resource Passes::ImGuiCubeTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassData
    {
        Resource Texture{};
        StringId Name{};
    };
    struct Context
    {
        u32 Layer{0};
    };
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.Texture = graph.ReadImage(textureIn, Pixel | Sampled);
            passData.Name = name;
            graph.HasSideEffect();
        },
        [=](const PassData& passData, FrameContext&, const Graph& graph)
        {
            CPU_PROFILE_FRAME("ImGui Texture")
            GPU_PROFILE_FRAME("ImGui Texture")

            Context& context = graph.GetOrCreateBlackboardValue<Context>();
            auto&& [texture, description] = graph.GetImageWithDescription(passData.Texture);

            ASSERT(description.Kind == ImageKind::Cubemap, "Only cubemap textures are supported")

            ImGui::Begin(passData.Name.AsString().c_str());
            ImGui::DragInt("Layer", (i32*)&context.Layer, 0.05f, 0, (i32)description.LayersDepth - 1);
            const glm::vec2 size = getTextureWindowSize(description);
            const Sampler sampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampEdge
            });
            ImGuiUI::Texture(ImageSubresource{
                    .Image = texture,
                    .Description = ImageSubresourceDescription{
                        .ImageViewKind = ImageViewKind::Image2d,
                        .LayerBase = (i8)context.Layer,
                        .Layers = 1
                    }
                },
                sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        }).Texture;
}

RG::Resource Passes::ImGuiTexture3d::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture,
    ChannelComposition channelComposition)
{
    return addToGraph(name, renderGraph, renderGraph.Import("In"_hsv, texture, ImageLayout::Readonly),
        channelComposition);
}

RG::Resource Passes::ImGuiTexture3d::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct Context
    {
        i32 Slice{0};
    };
    return renderGraph.AddRenderPass<Resource>(name,
        [&](Graph& graph, Resource& passData)
        {
            Context& context = graph.GetOrCreateBlackboardValue<Context>(name.Hash());
            auto& texture3dDescription = graph.GetImageDescription(textureIn);
            const u32 depth = texture3dDescription.GetDepth();
            const f32 sliceNormalized = ((f32)context.Slice + 0.5f) / (f32)depth;
            const Resource slice = Texture3dToSlice::addToGraph(name.Concatenate(".ToSlice"),
                renderGraph, textureIn, sliceNormalized, channelComposition);
            passData = imgui2dTexturePass(name, graph, slice, [&, depth=depth]()
            {
                ImGui::DragInt("Slice", &context.Slice, 0.1f, 0, (i32)depth - 1);
            });
            graph.HasSideEffect();
        },
        [=](const Resource&, FrameContext&, const Graph&)
        {
        });
}

RG::Resource Passes::ImGuiArrayTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture,
    DrawAs drawAs, ChannelComposition channelComposition)
{
    return addToGraph(name, renderGraph, renderGraph.Import("In"_hsv, texture, ImageLayout::Readonly), drawAs,
        channelComposition);
}

RG::Resource Passes::ImGuiArrayTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    DrawAs drawAs, ChannelComposition channelComposition)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct Context
    {
        i32 Slice{0};
    };
    return renderGraph.AddRenderPass<Resource>(name,
        [&](Graph& graph, Resource& passData)
        {
            auto& textureDescription = graph.GetImageDescription(textureIn);
            const i8 layers = textureDescription.GetLayers();
            if (drawAs == DrawAs::Slice)
            {
                Context& context = graph.GetOrCreateBlackboardValue<Context>(name.Hash());
                const Resource slice = TextureArrayToSlice::addToGraph(name.Concatenate(".ToSlice"),
                    renderGraph, textureIn, context.Slice, channelComposition);
                passData = imgui2dTexturePass(name, graph, slice, [&, layers=layers]()
                {
                    ImGui::DragInt("Slice", &context.Slice, 0.1f, 0, (i32)layers - 1);
                });
            }
            else
            {
                const Resource atlas = TextureArrayToAtlas::addToGraph(name.Concatenate(".ToAtlas"),
                    renderGraph, textureIn, channelComposition);
                passData = imgui2dTexturePass(name, graph, atlas, []()
                {
                });
            }
            graph.HasSideEffect();
        },
        [=](const Resource&, FrameContext&, const Graph&)
        {
        });
}
