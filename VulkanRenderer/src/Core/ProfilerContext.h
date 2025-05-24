#pragma once

#include "Settings.h"
#include "Rendering/CommandBuffer.h"

#include <array>

struct SourceLocationData
{
    const char* Name;
    const char* Function;
    const char* File;
    u32 Line;
    u32 Color;
};

struct ProfilerScopedZoneCpu
{
    ProfilerScopedZoneCpu(const SourceLocationData& data);
    ~ProfilerScopedZoneCpu();
    alignas (std::byte) std::byte Impl;
};

struct ProfilerScopedZoneGpu
{
    ProfilerScopedZoneGpu(const SourceLocationData& data);
    ~ProfilerScopedZoneGpu();
    static constexpr u64 IMPL_SIZE_BYTES = 24;
    alignas (std::max_align_t) std::byte Impl[IMPL_SIZE_BYTES];
};

class ProfilerContext
{
    friend class Device;
    friend class DeviceInternal;
    using Ctx = void*;
public:
    static ProfilerContext* Get();
    void Init(const std::array<CommandBuffer, BUFFERED_FRAMES>& cmds);
    void Shutdown();

    Ctx GraphicsContext();
    
    void NextFrame();
    void Collect();
private:
    std::array<Ctx, BUFFERED_FRAMES> m_GraphicsContexts{};
    std::array<CommandBuffer, BUFFERED_FRAMES> m_GraphicsCommandBuffers{};
    u32 m_CurrentFrame{0};
};

#define PROFILER_SOURCE_LOC_DATA(name) static constexpr SourceLocationData C_CONCAT(__source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (u32)__LINE__, 0 };
#define CPU_PROFILE_FRAME(name) PROFILER_SOURCE_LOC_DATA(name) ProfilerScopedZoneCpu cpuZone(C_CONCAT(__source_location,__LINE__));
#define GPU_PROFILE_FRAME(name) PROFILER_SOURCE_LOC_DATA(name) ProfilerScopedZoneGpu gpuZone(C_CONCAT(__source_location,__LINE__));
#define GPU_COLLECT_PROFILE_FRAMES() ProfilerContext::Get()->Collect();