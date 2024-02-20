#pragma once

#include "Rendering/Shader.h"

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

