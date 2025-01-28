#pragma once

#include "Settings.h"
#include "Rendering/CommandBuffer.h"

#include <array>

namespace tracy
{
    class VkCtx;
}

class ProfilerContext
{
    friend class Device;
    using TracyVkCtx = tracy::VkCtx*;
public:
    static ProfilerContext* Get();
    void Init(const std::array<CommandBuffer, BUFFERED_FRAMES>& cmds);
    void Shutdown();

    TracyVkCtx GraphicsContext();
    
    void NextFrame();
private:
    std::array<TracyVkCtx, BUFFERED_FRAMES> m_GraphicsContexts{};
    std::array<CommandBuffer, BUFFERED_FRAMES> m_GraphicsCommandBuffers{};
    u32 m_CurrentFrame{0};
};
