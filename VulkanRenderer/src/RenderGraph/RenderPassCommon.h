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

    struct BindlessTexturesPipelineData
    {
        ShaderPipeline Pipeline;
        ShaderDescriptors ImmutableSamplerDescriptors;
        ShaderDescriptors ResourceDescriptors;
        ShaderDescriptors TextureDescriptors;
    };

    struct PipelineDataDescriptorSet
    {
        ShaderPipeline Pipeline;
        ShaderDescriptorSet Descriptors;
    };
}



