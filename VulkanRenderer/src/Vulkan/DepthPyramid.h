#pragma once
#include <array>

#include "Image.h"
#include "Shader.h"
#include "types.h"

struct ComputeDilateData;
struct ComputeReprojectionData;
struct ComputeDepthPyramidData;
class ShaderPipeline;
class ShaderPipelineTemplate;
class CommandBuffer;

class DepthPyramid
{
    static constexpr u32 MAX_DEPTH = 16;
public:
    DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
        ComputeDepthPyramidData* computeDepthPyramidData);
    ~DepthPyramid();

    void ComputePyramid(const Image& depthImage, const CommandBuffer& cmd);

    const Image& GetTexture() const { return m_PyramidDepth; }
    VkSampler GetSampler() const { return m_Sampler; }
private:
    static VkSampler CreateSampler();
    static Image CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage);
    static std::array<VkImageView, MAX_DEPTH> CreateViews(const Image& pyramidImage);
    
    void CreateDescriptorSets(const Image& depthImage);
    void FillPyramid(const CommandBuffer& cmd, const Image& depthImage);
private:
    Image m_PyramidDepth;
    VkSampler m_Sampler{VK_NULL_HANDLE};
    
    std::array<VkImageView, MAX_DEPTH> m_MipMapViews{VK_NULL_HANDLE};
    ComputeDepthPyramidData* m_ComputeDepthPyramidData;
    std::array<ShaderDescriptorSet, MAX_DEPTH> m_DepthPyramidDescriptors;
};
