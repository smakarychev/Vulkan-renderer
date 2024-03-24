#pragma once

#include "types.h"

#include <vector>

#include "FormatTraits.h"
#include "RenderHandle.h"
#include "RenderObject.h"

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class DriverResources; \
    friend class DeletionQueue; \
    friend class RenderCommand;

#define GPU_PROFILE_FRAME(name) TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), name)
#define CPU_PROFILE_FRAME(name) ZoneScopedN(name);

class Buffer;
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

struct RenderingDetails
{
    std::vector<Format> ColorFormats;
    // todo: make it an std::optional?
    Format DepthFormat;
};