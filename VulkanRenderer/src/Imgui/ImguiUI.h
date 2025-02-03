#pragma once

#include <memory>
#include <glm/vec2.hpp>
#include <imgui/imgui.h>

#include "Settings.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/RenderingInfo.h"  
#include "Rendering/Image/ImageTraits.h"
#include "Rendering/Image/Sampler.h"

class RenderCommandList;
struct ImageSubresource;

class ImGuiUI
{
    FRIEND_INTERNAL
public:
    static void BeginFrame(RenderCommandList& cmdList, u32 frameNumber);
    static void EndFrame(RenderCommandList& cmdList, RenderingInfo renderingInfo);

    static void Texture(const ImageSubresource& texture, Sampler sampler, ImageLayout layout, const glm::uvec2& size);
private:
    static void ClearFrameResources(u32 frameNumber);
private:
    static u32 s_FrameNumber;
    static std::array<std::vector<ImTextureID>, BUFFERED_FRAMES> s_Textures;
};
