#pragma once

#include "Rendering/Shader.h"

struct ProcessingPipeline
{
    ShaderPipeline Pipeline{};
    ShaderDescriptors SamplerDescriptors{};
    ShaderDescriptors ResourceDescriptors{};
};

class ProcessingResources
{
public:
    static DescriptorArenaAllocators Allocators();
    static ProcessingPipeline CreatePipeline(ShaderPipelineTemplate* pipelineTemplate);
};
