#include "ImGuiTexturePass.h"

#include "imgui/imgui.h"
#include "Imgui/ImguiUI.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/Texture3dToSliceBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace
{
    glm::vec2 getTextureWindowSize(const TextureDescription& texture)
    {
        ImVec2 availableRegion = ImGui::GetContentRegionAvail();
        glm::vec2 size = {availableRegion.x, availableRegion.y};
        // save the aspect ratio of image
        f32 aspect = texture.AspectRatio();
        if (aspect > 1.0f)
            size.y = size.x / aspect;
        else
            size.x = size.y * aspect;

        return size;
    }
}

RG::Pass& Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal("In"_hsv, texture));
}

RG::Pass& Passes::ImGuiTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassData
    {
        Resource Texture{};
        StringId Name{};
    };
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.Texture = graph.Read(textureIn, Pixel | Sampled);
            passData.Name = name;
            graph.HasSideEffect();
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("ImGui Texture")
            GPU_PROFILE_FRAME("ImGui Texture")

            auto&& [texture, description] = resources.GetTextureWithDescription(passData.Texture);
            
            ImGui::Begin(passData.Name.AsString().c_str());
            glm::vec2 size = getTextureWindowSize(description);
            Sampler sampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampEdge});
            ImGuiUI::Texture(ImageSubresource{.Image = texture}, sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}

RG::Pass& Passes::ImGuiCubeTexture::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal("In"_hsv, texture));
}

RG::Pass& Passes::ImGuiCubeTexture::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn)
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
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.Texture = graph.Read(textureIn, Pixel | Sampled);
            passData.Name = name;
            graph.HasSideEffect();
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("ImGui Texture")
            GPU_PROFILE_FRAME("ImGui Texture")

            Context& context = resources.GetOrCreateValue<Context>();
            auto&& [texture, description] = resources.GetTextureWithDescription(passData.Texture);

            ASSERT(description.Kind == ImageKind::Cubemap, "Only cubemap textures are supported")
            
            ImGui::Begin(passData.Name.AsString().c_str());
            ImGui::DragInt("Layer", (i32*)&context.Layer, 0.05f, 0, (i32)description.LayersDepth - 1);
            glm::vec2 size = getTextureWindowSize(description);
            Sampler sampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampEdge});
            ImGuiUI::Texture(ImageSubresource{
                .Image = texture,
                .Description = ImageSubresourceDescription{
                    .ImageViewKind = ImageViewKind::Image2d,
                    .LayerBase = (i8)context.Layer,
                    .Layers = 1}},
                sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}

namespace
{
    RG::Resource texture3dTo2dSlicePass(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
        f32 sliceNormalized)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        struct PassData
        {
            Resource Texture3d{};
            Resource Slice{};
        };
        auto& pass = renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Texture3dToSlice.Setup")

                graph.SetShader("texture3d-to-slice.shader");

                auto& texture3dDescription = Resources(graph).GetTextureDescription(textureIn);
                passData.Slice = graph.CreateResource("Slice"_hsv,
                    GraphTextureDescription{
                        .Width = texture3dDescription.Width,
                        .Height = texture3dDescription.Height,
                        .Format = Format::RGBA16_FLOAT});

                passData.Texture3d = graph.Read(textureIn, Pixel | Sampled);
                passData.Slice = graph.RenderTarget(passData.Slice, AttachmentLoad::Load, AttachmentStore::Store);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Texture3dToSlice")
                GPU_PROFILE_FRAME("Texture3dToSlice")

                const Shader& shader = resources.GetGraph()->GetShader();
                Texture3dToSliceShaderBindGroup bindGroup(shader);

                bindGroup.SetTexture({.Image = resources.GetTexture(passData.Texture3d)}, ImageLayout::Readonly);

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                cmd.PushConstants({
                	.PipelineLayout = shader.GetLayout(), 
                	.Data = {sliceNormalized}});
                cmd.Draw({.VertexCount = 3});
            });

        return renderGraph.GetBlackboard().Get<PassData>(pass).Slice;
    }
}

RG::Pass& Passes::ImGuiTexture3d::addToGraph(StringId name, RG::Graph& renderGraph, Texture texture)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal("In"_hsv, texture));
}

RG::Pass& Passes::ImGuiTexture3d::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassData
    {
        Resource Texture{};
        StringId Name{};
        u32 Depth{0};
    };
    struct Context
    {
        i32 Slice{0};
    };
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            Context& context = graph.GetOrCreateBlackboardValue<Context>();
            auto& texture3dDescription = Resources(graph).GetTextureDescription(textureIn);
            u32 depth = texture3dDescription.GetDepth();
            f32 sliceNormalized = ((f32)context.Slice + 0.5f) / (f32)depth;
            Resource slice = texture3dTo2dSlicePass(name.Concatenate(".ToSlice"),
                renderGraph, textureIn, sliceNormalized);
            passData.Texture = graph.Read(slice, Pixel | Sampled);
            
            passData.Name = name;
            passData.Depth = depth;
            graph.HasSideEffect();
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("ImGui Texture")
            GPU_PROFILE_FRAME("ImGui Texture")

            Context& context = resources.GetOrCreateValue<Context>();
            auto&& [slice, description] = resources.GetTextureWithDescription(passData.Texture);
            
            ImGui::Begin(passData.Name.AsString().c_str());
            ImGui::DragInt("Slice", &context.Slice, 0.1f, 0, (i32)passData.Depth - 1);
            glm::vec2 size = getTextureWindowSize(description);
            Sampler sampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampEdge});
            ImGuiUI::Texture(ImageSubresource{.Image = slice}, sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}
