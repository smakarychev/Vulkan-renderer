#include "ProcessingResources.h"

#include "Rendering/Descriptors.h"

DescriptorArenaAllocators ProcessingResources::Allocators()
{
    DescriptorArenaAllocator samplerAllocator = DescriptorArenaAllocator::Builder()
        .Kind(DescriptorAllocatorKind::Samplers)
        .Residence(DescriptorAllocatorResidence::CPU)
        .ForTypes({DescriptorType::Sampler})
        .Count(1)
        .Build();

    DescriptorArenaAllocator resourceAllocator = DescriptorArenaAllocator::Builder()
        .Kind(DescriptorAllocatorKind::Resources)
        .Residence(DescriptorAllocatorResidence::CPU)
        .ForTypes({DescriptorType::Image})
        .Count(32)
        .Build();

    DescriptorArenaAllocators allocators(resourceAllocator, samplerAllocator);

    return allocators;
}

ProcessingPipeline ProcessingResources::CreatePipeline(ShaderPipelineTemplate* pipelineTemplate)
{
    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(pipelineTemplate)
        .UseDescriptorBuffer()
        .Build();
    ShaderDescriptors samplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(pipelineTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();
    ShaderDescriptors resourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(pipelineTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    return {
        .Pipeline = pipeline,
        .SamplerDescriptors = samplerDescriptors,
        .ResourceDescriptors = resourceDescriptors};
}
