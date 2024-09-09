#pragma once

#include "RGResource.h"
#include "Rendering/Shader.h"

class Camera;
class CommandBuffer;

namespace RG
{
    struct GlobalResources
    {
        u64 FrameNumberTick{0};
        glm::uvec2 Resolution{};
        const Camera* PrimaryCamera{nullptr}; 
        Resource PrimaryCameraGPU{};
        Resource ShadingSettings{};
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



