#pragma once
#include "RenderHandle.h"
#include "types.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Image/ImageUtility.h"

/* This class manages a descriptor set for bindless textures.
 * The max number of textures is fixed on creation,
 * if overflow happens, the oldest textures will be swapped with new ones
 * It does not handle deduplication
 */
class BindlessTextureDescriptorsRingBuffer
{
public:
    BindlessTextureDescriptorsRingBuffer(u32 maxCount, Descriptors descriptors);
    BindlessTextureDescriptorsRingBuffer(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer(BindlessTextureDescriptorsRingBuffer&&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(BindlessTextureDescriptorsRingBuffer&&) = delete;
    ~BindlessTextureDescriptorsRingBuffer() = default;

    u32 Size() const;
    u32 FreeSize() const;
    
    bool WillOverflow() const;
    TextureHandle AddTexture(Texture texture);
    void SetTexture(TextureHandle index, Texture texture);
    Texture GetTexture(TextureHandle index) const;
    TextureHandle GetDefaultTexture(Images::DefaultKind texture) const;
private:
    u32 GetNextIndex(u32 index) const;
    void UpdateDescriptor(Texture texture, u32 index) const;
private:
    u32 m_Head{0};
    u32 m_Tail{0};
    u32 m_MaxBindlessCount{0};
    Descriptors m_Descriptors{};

    std::array<TextureHandle, (u32)Images::DefaultKind::MaxVal> m_DefaultTextures;
    std::vector<Texture> m_Textures;
};
