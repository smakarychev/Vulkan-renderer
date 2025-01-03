#pragma once
#include "types.h"
#include "Rendering/Shader.h"

/* This class manages descriptor set for bindless textures.
 * The max amount of textures is fixed on creation,
 * if overflow happens, the oldest textures will be swapped with new ones
 * It does not handle deduplication
 * It does not store textures
 */
class BindlessTextureDescriptorsRingBuffer
{
public:
    BindlessTextureDescriptorsRingBuffer(u32 maxCount, const ShaderPipelineTemplate* pipelineTemplate);
    BindlessTextureDescriptorsRingBuffer(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer(BindlessTextureDescriptorsRingBuffer&&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(BindlessTextureDescriptorsRingBuffer&&) = delete;
    ~BindlessTextureDescriptorsRingBuffer() = default;

    ShaderDescriptors& GetDescriptors() { return m_BindlessDescriptorSet; }

    u32 Size() const;
    u32 FreeSize() const;
    
    bool WillOverflow() const;
    u32 AddTexture(const Texture& texture);
private:
    u32 GetNextIndex(u32 index) const;
private:
    u32 m_Head{0};
    u32 m_Tail{0};
    u32 m_MaxBindlessCount{0};
    ShaderDescriptors m_BindlessDescriptorSet;
};
