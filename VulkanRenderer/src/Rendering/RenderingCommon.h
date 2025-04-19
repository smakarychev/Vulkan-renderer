#pragma once

#include "types.h"

#define FRIEND_INTERNAL \
    friend class Device; \
    friend class DeviceResources; \
    friend class DeletionQueue; \

#define GPU_PROFILE_FRAME(name) TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Device::GetProfilerCommandBuffer(ProfilerContext::Get()), name)
#define CPU_PROFILE_FRAME(name) ZoneScopedN(name);

// todo: cleanup this entire file

enum class QueueKind {Graphics, Presentation, Compute};

struct DepthBias
{
    f32 Constant{0.0f};
    f32 Slope{0.0f};
};