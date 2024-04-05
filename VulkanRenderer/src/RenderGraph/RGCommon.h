#pragma once

#include "Rendering/Shader.h"

class RenderPassGeometry;
class CommandBuffer;

namespace RenderGraph
{
    struct GlobalResources
    {
        Resource MainCameraGPU{};
    };
    
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
        ShaderDescriptors MaterialDescriptors;
    };

    struct PipelineDataDescriptorSet
    {
        ShaderPipeline Pipeline;
        ShaderDescriptorSet Descriptors;
    };
}



