#pragma once

#include "RGResource.h"
#include "Rendering/Shader.h"

class CommandBuffer;

namespace RG
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



