#pragma once

#include "Rendering/Shader.h"

class RenderPassGeometry;
class CommandBuffer;

namespace RenderGraph
{
    struct PipelineData
    {
        ShaderPipeline Pipeline;
        ShaderDescriptorSet Descriptors;
    };
}



