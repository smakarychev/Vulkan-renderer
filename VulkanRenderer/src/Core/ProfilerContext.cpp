#include "ProfilerContext.h"

#include <client/TracyScoped.hpp>

#include "Vulkan/Device.h"
#include "Rendering/CommandBuffer.h"

static_assert(sizeof(SourceLocationData) == sizeof(tracy::SourceLocationData));

ProfilerScopedZoneCpu::ProfilerScopedZoneCpu(const SourceLocationData& data)
{
    static_assert(sizeof(Impl) >= sizeof(tracy::ScopedZone));
    new (&Impl) tracy::ScopedZone((const tracy::SourceLocationData*)&data);
}

ProfilerScopedZoneCpu::~ProfilerScopedZoneCpu()
{
    std::launder((tracy::ScopedZone*)&Impl)->~ScopedZone();
}

ProfilerScopedZoneGpu::ProfilerScopedZoneGpu(const SourceLocationData& data)
{
    Device::CreateGpuProfileFrame(*this, data);
}

ProfilerScopedZoneGpu::~ProfilerScopedZoneGpu()
{
    Device::DestroyGpuProfileFrame(*this);
}

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

ProfilerContext::Ctx ProfilerContext::GraphicsContext()
{
    return m_GraphicsContexts[m_CurrentFrame];
}

void ProfilerContext::NextFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % BUFFERED_FRAMES;
}

void ProfilerContext::Collect()
{
    Device::CollectGpuProfileFrames();
}

