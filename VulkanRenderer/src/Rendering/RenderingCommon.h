#pragma once

#include "types.h"

#include <vector>

#include "DescriptorSetTraits.h"
#include "FormatTraits.h"
#include "RenderHandle.h"
#include "RenderObject.h"

#define FRIEND_INTERNAL \
    friend class Driver; \
    friend class DriverResources; \
    friend class DeletionQueue; \
    friend class RenderCommand;

class Buffer;
class Image;

struct IndirectCommand
{
    u32 IndexCount;
    u32 InstanceCount;
    u32 FirstIndex;
    i32 VertexOffset;
    u32 FirstInstance;
    RenderHandle<RenderObject> RenderObject;
};

struct VertexInputDescription
{
    struct Binding
    {
        u32 Index;
        u32 StrideBytes;
    };
    struct Attribute
    {
        u32 Index;
        u32 BindingIndex;
        Format Format;
        u32 OffsetBytes;
    };
    std::vector<Binding> Bindings;
    std::vector<Attribute> Attributes;
};

enum class PrimitiveKind
{
    Triangle, Point
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

struct PipelineSpecializationInfo
{
    struct ShaderSpecialization
    {
        u32 Id;
        u32 SizeBytes;
        u32 Offset;
        ShaderStage ShaderStages;
    };
    std::vector<ShaderSpecialization> ShaderSpecializations;
    std::vector<u8> Buffer;
};