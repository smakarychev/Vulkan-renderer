#pragma once

#include "types.h"

#include <vector>

#include "FormatTraits.h"
#include "RenderHandle.h"
#include "RenderObject.h"
#include "Core/core.h"

#define FRIEND_INTERNAL \
    friend class Device; \
    friend class DeviceResources; \
    friend class DeletionQueue; \
    friend class RenderCommand;

#define GPU_PROFILE_FRAME(name) TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Device::GetProfilerCommandBuffer(ProfilerContext::Get()), name)
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

struct DepthBias
{
    f32 Constant{0.0f};
    f32 Slope{0.0f};
};

enum class DrawFeatures
{
    None        = 0,
    Positions   = BIT(1),
    Normals     = BIT(2),
    Tangents    = BIT(3),
    UV          = BIT(4),
    Materials   = BIT(5),
    Textures    = BIT(6),
    SSAO        = BIT(7),
    IBL         = BIT(8),

    // does graphics use 'u_triangles' buffer
    Triangles   = BIT(9),

    // positions, normals, uvs (tangents are not used)  
    MainAttributes = Positions | Normals | UV,

    // positions, normals, tangents, uvs
    AllAttributes = MainAttributes | Tangents,

    // positions and uvs for texture fetch
    AlphaTest = Positions | UV | Materials | Textures,
        
    // all attributes + materials and textures
    Shaded = AllAttributes | Materials | Textures,

    // materials + all ibl textures
    ShadedIBL = Shaded | IBL,
};
CREATE_ENUM_FLAGS_OPERATORS(DrawFeatures)