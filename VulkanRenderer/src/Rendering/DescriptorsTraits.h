#pragma once
#include "core.h"

enum class DescriptorType
{
    Sampler = 0,
    Image,
    ImageStorage,
    TexelUniform,
    TexelStorage,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    Input
};

enum class DescriptorFlags : u8
{
    None = 0,
    
    UpdateAfterBind     = BIT(1),
    UpdateUnusedPending = BIT(2),
    PartiallyBound      = BIT(3),
    VariableCount       = BIT(4)
};

CREATE_ENUM_FLAGS_OPERATORS(DescriptorFlags)

enum class DescriptorLayoutFlags
{
    None = 0,

    UpdateAfterBind = BIT(1),
    DescriptorBuffer = BIT(2),
    EmbeddedImmutableSamplers = BIT(3),
};

CREATE_ENUM_FLAGS_OPERATORS(DescriptorLayoutFlags)

enum class DescriptorPoolFlags
{
    None = 0,
    
    UpdateAfterBind = BIT(1),
    HostOnly        = BIT(2)
};

CREATE_ENUM_FLAGS_OPERATORS(DescriptorPoolFlags)


enum class ShaderStage : u8
{
    None = 0,
    
    Vertex  = BIT(1),
    Pixel   = BIT(2),
    Compute = BIT(3)
};

CREATE_ENUM_FLAGS_OPERATORS(ShaderStage)