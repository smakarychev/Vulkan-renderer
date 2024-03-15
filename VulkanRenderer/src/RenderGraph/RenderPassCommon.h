#pragma once

#include "Rendering/Shader.h"

class RenderPassGeometry;
class CommandBuffer;

namespace RenderGraph
{
    struct PipelineData
    {
        ShaderPipeline Pipeline;
        ShaderDescriptors SamplerDescriptors;
        ShaderDescriptors ResourceDescriptors;
    };

    struct PipelineDataDescriptorSet
    {
        ShaderPipeline Pipeline;
        ShaderDescriptorSet Descriptors;
    };
}



