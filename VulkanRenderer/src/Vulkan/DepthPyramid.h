﻿#pragma once
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
    static constexpr u32 MAX_DEPTH = 16;
public:
    DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
        ComputeDepthPyramidData* computeDepthPyramidData);
    ~DepthPyramid();

    bool IsPendingTransition() const { return m_IsPendingTransition; }
    void SubmitLayoutTransition(const CommandBuffer& cmd, const BufferSubmitTimelineSyncInfo& syncInfo);
    
    void Compute(const Image& depthImage, const CommandBuffer& cmd);

    const Image& GetTexture() const { return m_PyramidDepth; }
    VkSampler GetSampler() const { return m_Sampler; }
private:
    static VkSampler CreateSampler();
    static Image CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage);
    static std::array<VkImageView, MAX_DEPTH> CreateViews(const Image& pyramidImage);
    
    void CreateDescriptorSets(const Image& depthImage);
    void Fill(const CommandBuffer& cmd, const Image& depthImage);
private:
    Image m_PyramidDepth;
    bool m_IsPendingTransition{true};
    
    VkSampler m_Sampler{VK_NULL_HANDLE};
    
    std::array<VkImageView, MAX_DEPTH> m_MipMapViews{VK_NULL_HANDLE};
    ComputeDepthPyramidData* m_ComputeDepthPyramidData;
    std::array<ShaderDescriptorSet, MAX_DEPTH> m_DepthPyramidDescriptors;
};