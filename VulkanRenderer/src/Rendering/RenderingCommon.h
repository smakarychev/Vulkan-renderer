#pragma once

#include "types.h"

#include <vector>

#include "FormatTraits.h"
#include "RenderHandle.h"
#include "RenderObject.h"
#include "core.h"

#define FRIEND_INTERNAL \
    friend class Device; \
    friend class DeviceResources; \
    friend class DeletionQueue; \
    friend class RenderCommand;

#define GPU_PROFILE_FRAME(name) TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Device::GetProfilerCommandBuffer(ProfilerContext::Get()), name)
#define CPU_PROFILE_FRAME(name) ZoneScopedN(name);

class Image;

struct IndirectDrawCommand
{
    u32 IndexCount;
    u32 InstanceCount;
    u32 FirstIndex;
    i32 VertexOffset;
    u32 FirstInstance;
    RenderHandle<RenderObject> RenderObject;
};

struct IndirectDispatchCommand
{
    u32 GroupX;
    u32 GroupY;
    u32 GroupZ;
};

enum class QueueKind {Graphics, Presentation, Compute};

enum class AlphaBlending {None, Over};

struct BufferCopyInfo
{
    u64 SizeBytes;
    u64 SourceOffset;
    u64 DestinationOffset;
};

struct DepthBias
{
    f32 Constant{0.0f};
    f32 Slope{0.0f};
};