#include "ImGuiTexturePass.h"

#include "imgui/imgui.h"
#include "Imgui/ImguiUI.h"
#include "RenderGraph/RenderGraph.h"

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
            ImVec2 availableRegion = ImGui::GetContentRegionAvail();
            glm::vec2 size = {availableRegion.x, availableRegion.y};
            // save the aspect ratio of image
            f32 aspect = texture.Description().AspectRatio();
            if (aspect > 1.0f)
                size.y = size.x / aspect;
            else
                size.x = size.y * aspect;

            Sampler sampler = Sampler::Builder().WrapMode(SamplerWrapMode::ClampEdge).Build();
            ImGuiUI::Texture(texture.Subresource(), sampler, ImageLayout::Readonly,
                glm::uvec2(size));
            ImGui::End();
        });

    return pass;
}