#include "ImguiUI.h"

#include "types.h"

#include <array>

#include <volk.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

u32 ImGuiUI::s_FrameNumber{0};
std::array<std::vector<ImTextureID>, BUFFERED_FRAMES> ImGuiUI::s_Textures{};

void ImGuiUI::BeginFrame(u32 frameNumber)
{
    CPU_PROFILE_FRAME("ImGui begin frame")

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    s_FrameNumber = frameNumber;
    ClearFrameResources(frameNumber);
}

void ImGuiUI::EndFrame(const CommandBuffer& cmd, const RenderingInfo& renderingInfo)
{
    CPU_PROFILE_FRAME("ImGui end frame")

    ImGui::Render();
    
    RenderCommand::BeginRendering(cmd, renderingInfo);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Driver::Resources()[cmd].CommandBuffer);

    RenderCommand::EndRendering(cmd);
}

void ImGuiUI::Texture(const ImageSubresource& texture, Sampler sampler, ImageLayout layout, const glm::uvec2& size)
{
    ImTextureID textureId = Driver::CreateImGuiImage(texture, sampler, layout);
    ImGui::Image(textureId, ImVec2{(f32)size.x, (f32)size.y});
    s_Textures[s_FrameNumber].push_back(textureId);
}

void ImGuiUI::ClearFrameResources(u32 frameNumber)
{
    for (auto& texture : s_Textures[frameNumber])
        Driver::DestroyImGuiImage(texture);
    s_Textures[frameNumber].clear();
}

