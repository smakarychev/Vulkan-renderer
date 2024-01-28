#pragma once

#include "HandleArray.h"
#include "Vulkan/Image.h"
#include "Vulkan/Shader.h"

class RenderPassGeometry;
class CommandBuffer;

struct RenderPassPipelineData
{
    ShaderPipeline Pipeline;
    ShaderDescriptorSet Descriptors;
};

struct RenderPassExecutionContext
{
    CommandBuffer* Cmd;
    u32 FrameNumber;
    
    RenderPassGeometry* Geometry;
};

struct RenderPassResources
{
    HandleArray<Texture> Textures;
    HandleArray<RenderPassPipelineData> Pipelines;
};