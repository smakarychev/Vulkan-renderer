#pragma once
#include <array>

#include "Settings.h"

namespace tracy
{
    class VkCtx;
}

class CommandBuffer;

class ProfilerContext
{
    friend class Driver;
    using TracyVkCtx = tracy::VkCtx*;
public:
    static ProfilerContext* Get();
    void Init(const std::array<CommandBuffer*, BUFFERED_FRAMES>& cmds);
    void Shutdown();

    TracyVkCtx GraphicsContext();
    
    void NextFrame();
private:
    std::array<TracyVkCtx, BUFFERED_FRAMES> m_GraphicsContexts{};
    std::array<CommandBuffer*, BUFFERED_FRAMES> m_GraphicsCommandBuffers{};
    u32 m_CurrentFrame{0};
};
