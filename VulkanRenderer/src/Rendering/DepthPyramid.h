#pragma once
#include <array>

#include "Image.h"
#include "Shader.h"
#include "types.h"

struct BufferSubmitTimelineSyncInfo;
struct ComputeDilateData;
struct ComputeReprojectionData;
struct ComputeDepthPyramidData;
class ShaderPipeline;
class ShaderPipelineTemplate;
class CommandBuffer;

class DepthPyramid
{
    static constexpr u32 MAX_MIPMAP_COUNT = 16;
public:
    DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
        ComputeDepthPyramidData* computeDepthPyramidData);
    ~DepthPyramid();

    void Compute(const Image& depthImage, const CommandBuffer& cmd, DeletionQueue& deletionQueue);

    const Image& GetTexture() const { return m_PyramidDepth; }
    Sampler GetSampler() const { return m_Sampler; }
private:
    static Sampler CreateSampler();
    
    Image CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage);
    void CreateDescriptorSets(const Image& depthImage);
    void Fill(const CommandBuffer& cmd, const Image& depthImage, DeletionQueue& deletionQueue);
private:
    Image m_PyramidDepth;
    Sampler m_Sampler;
    std::array<ImageViewHandle, DepthPyramid::MAX_MIPMAP_COUNT> m_MipmapViewHandles;
    
    ComputeDepthPyramidData* m_ComputeDepthPyramidData;
    std::array<ShaderDescriptorSet, MAX_MIPMAP_COUNT> m_DepthPyramidDescriptors;
};
