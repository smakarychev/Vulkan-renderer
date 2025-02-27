#pragma once
#include "types.h"
#include "Rendering/Shader/Shader.h"

class Shader;
/* This class manages descriptor set for bindless textures.
 * The max amount of textures is fixed on creation,
 * if overflow happens, the oldest textures will be swapped with new ones
 * It does not handle deduplication
 */
class BindlessTextureDescriptorsRingBuffer
{
public:
    BindlessTextureDescriptorsRingBuffer(u32 maxCount, const Shader& shader);
    BindlessTextureDescriptorsRingBuffer(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(const BindlessTextureDescriptorsRingBuffer&) = delete;
    BindlessTextureDescriptorsRingBuffer(BindlessTextureDescriptorsRingBuffer&&) = delete;
    BindlessTextureDescriptorsRingBuffer& operator=(BindlessTextureDescriptorsRingBuffer&&) = delete;
    ~BindlessTextureDescriptorsRingBuffer() = default;

    const Shader& GetMaterialsShader() const { return *m_MaterialsShader; }

    u32 Size() const;
    u32 FreeSize() const;
    
    bool WillOverflow() const;
    u32 AddTexture(Texture texture);
    u32 GetDefaultTexture(ImageUtils::DefaultTexture texture) const;
private:
    u32 GetNextIndex(u32 index) const;
private:
    u32 m_Head{0};
    u32 m_Tail{0};
    u32 m_MaxBindlessCount{0};
    const Shader* m_MaterialsShader;

    std::array<u32, (u32)ImageUtils::DefaultTexture::MaxVal> m_DefaultTextures;
    std::vector<Texture> m_Textures;
};
