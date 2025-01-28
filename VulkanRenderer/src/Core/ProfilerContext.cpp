#include "ProfilerContext.h"

#include "Vulkan/Device.h"
#include "Rendering/CommandBuffer.h"

ProfilerContext* ProfilerContext::Get()
{
    static ProfilerContext profilerContext = {};
    return &profilerContext;
}

void ProfilerContext::Init(const std::array<CommandBuffer, BUFFERED_FRAMES>& cmds)
{
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        m_GraphicsContexts[i] = Device::CreateTracyGraphicsContext(cmds[i]);
    m_GraphicsCommandBuffers = cmds;
}

void ProfilerContext::Shutdown()
{
    for (auto ctx : m_GraphicsContexts)
        Device::DestroyTracyGraphicsContext(ctx);
}

TracyVkCtx ProfilerContext::GraphicsContext()
{
    return m_GraphicsContexts[m_CurrentFrame];
}

void ProfilerContext::NextFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % BUFFERED_FRAMES;
}

