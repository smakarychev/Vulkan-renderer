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

    void Compute(const Image& depthImage, const CommandBuffer& cmd);

    const Image& GetTexture() const { return m_PyramidDepth; }
    Sampler GetSampler() const { return m_Sampler; }
private:
    static Sampler CreateSampler();
    static Image CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage);
    
    ImageViewList CreateViews(const Image& pyramidImage);
    void CreateDescriptorSets(const Image& depthImage);
    void Fill(const CommandBuffer& cmd, const Image& depthImage);
private:
    Image m_PyramidDepth;
    Sampler m_Sampler;
    ImageViewList m_MipmapViews;
    std::array<ImageViewHandle, DepthPyramid::MAX_MIPMAP_COUNT> m_MipmapViewHandles;
    
    ComputeDepthPyramidData* m_ComputeDepthPyramidData;
    std::array<ShaderDescriptorSet, MAX_MIPMAP_COUNT> m_DepthPyramidDescriptors;
};
