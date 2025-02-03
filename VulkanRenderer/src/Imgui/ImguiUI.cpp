#include "ImguiUI.h"

#include "types.h"

#include "Vulkan/Device.h"

#include "Rendering/Commands/RenderCommandList.h"

u32 ImGuiUI::s_FrameNumber{0};
std::array<std::vector<ImTextureID>, BUFFERED_FRAMES> ImGuiUI::s_Textures{};

void ImGuiUI::BeginFrame(RenderCommandList& cmdList, u32 frameNumber)
{
    CPU_PROFILE_FRAME("ImGui begin frame")

    cmdList.BeginImGuiRendering({});

    s_FrameNumber = frameNumber;
    ClearFrameResources(frameNumber);
}

void ImGuiUI::EndFrame(RenderCommandList& cmdList, RenderingInfo renderingInfo)
{
    CPU_PROFILE_FRAME("ImGui end frame")

    cmdList.EndImGuiRendering({.RenderingInfo = renderingInfo});
}

void ImGuiUI::Texture(const ImageSubresource& texture, Sampler sampler, ImageLayout layout, const glm::uvec2& size)
{
    ImTextureID textureId = Device::CreateImGuiImage(texture, sampler, layout);
    ImGui::Image(textureId, ImVec2{(f32)size.x, (f32)size.y});
    s_Textures[s_FrameNumber].push_back(textureId);
}

void ImGuiUI::ClearFrameResources(u32 frameNumber)
{
    for (auto& texture : s_Textures[frameNumber])
        Device::DestroyImGuiImage(texture);
    s_Textures[frameNumber].clear();
}

