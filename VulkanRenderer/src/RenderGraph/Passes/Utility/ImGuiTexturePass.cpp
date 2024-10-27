#include "ImGuiTexturePass.h"

#include "imgui/imgui.h"
#include "Imgui/ImguiUI.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

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

RG::Pass& Passes::ImGuiTexture::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& texture)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal(std::string{name} + ".In", texture));
}

RG::Pass& Passes::ImGuiTexture::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassData
    {
        Resource Texture{};
        std::string Name{};
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

            const Texture& texture = resources.GetTexture(passData.Texture);
            
            ImGui::Begin(passData.Name.c_str());
            glm::vec2 size = getTextureWindowSize(texture.Description());
            Sampler sampler = Sampler::Builder().WrapMode(SamplerWrapMode::ClampEdge).Build();
            ImGuiUI::Texture(texture.Subresource(), sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}

namespace
{
    RG::Resource texture3dTo2dSlicePass(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn,
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

                graph.SetShader("../assets/shaders/texture3d-to-slice.shader");

                auto& texture3dDescription = Resources(graph).GetTextureDescription(textureIn);
                passData.Slice = graph.CreateResource(std::format("{}.Slice", name),
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
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                resourceDescriptors.UpdateBinding("u_texture",
                    resources.GetTexture(passData.Texture3d).BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));

                auto& cmd = frameContext.Cmd;
                samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
                pipeline.BindGraphics(cmd);
                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), sliceNormalized);
                resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
                
                RenderCommand::Draw(cmd, 3);
            });

        return renderGraph.GetBlackboard().Get<PassData>(pass).Slice;
    }
}

RG::Pass& Passes::ImGuiTexture3d::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& texture)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal(std::string{name} + ".In", texture));
}

RG::Pass& Passes::ImGuiTexture3d::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassData
    {
        Resource Texture{};
        std::string Name{};
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
            u32 depth = TextureDescription::GetDepth(texture3dDescription);
            f32 sliceNormalized = ((f32)context.Slice + 0.5f) / (f32)depth;
            Resource slice = texture3dTo2dSlicePass(std::format("{}.ToSlice", name),
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
            const Texture& slice = resources.GetTexture(passData.Texture);
            
            ImGui::Begin(passData.Name.c_str());
            ImGui::DragInt("Slice", &context.Slice, 0.1f, 0, passData.Depth - 1);
            glm::vec2 size = getTextureWindowSize(slice.Description());
            Sampler sampler = Sampler::Builder().WrapMode(SamplerWrapMode::ClampEdge).Build();
            ImGuiUI::Texture(slice.Subresource(), sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}
