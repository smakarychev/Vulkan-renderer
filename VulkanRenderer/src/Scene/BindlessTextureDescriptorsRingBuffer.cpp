#include "BindlessTextureDescriptorsRingBuffer.h"

#include "Vulkan/Device.h"
#include "RenderGraph/Passes/Generated/MaterialsBindGroup.generated.h"

BindlessTextureDescriptorsRingBuffer::BindlessTextureDescriptorsRingBuffer(u32 maxCount, const Shader& shader)
    : m_MaxBindlessCount(maxCount), m_MaterialsShader(shader)
{
    for (u32 i = 0; i < m_DefaultTextures.size(); i++)
        m_DefaultTextures[i] = AddTexture(Images::Default::GetCopy(
            (Images::DefaultKind)i, Device::DeletionQueue()));
}

u32 BindlessTextureDescriptorsRingBuffer::Size() const
{
    return m_Tail >= m_Head ? m_Tail - m_Head : m_MaxBindlessCount - (m_Head - m_Tail);
}

u32 BindlessTextureDescriptorsRingBuffer::FreeSize() const
{
    return m_MaxBindlessCount - Size();
}

bool BindlessTextureDescriptorsRingBuffer::WillOverflow() const
{
    return FreeSize() == 0;
}

u32 BindlessTextureDescriptorsRingBuffer::AddTexture(Texture texture)
{
    MaterialsShaderBindGroup bindGroup(m_MaterialsShader);
    bindGroup.SetTextures({
            .Subresource = {.Image = texture},
            .Layout = ImageLayout::Readonly
        },
        m_Tail);

    const u32 toReturn = m_Tail;
    if (toReturn >= m_Textures.size())
        m_Textures.resize(toReturn + 1);
    m_Textures[toReturn] = texture;
    
    if (WillOverflow())
        m_Head = GetNextIndex(m_Head);

    m_Tail = GetNextIndex(m_Tail);

    return toReturn;
}

void BindlessTextureDescriptorsRingBuffer::SetTexture(u32 index, Texture texture)
{
    MaterialsShaderBindGroup bindGroup(m_MaterialsShader);
    bindGroup.SetTextures({
            .Subresource = {.Image = texture},
            .Layout = ImageLayout::Readonly
        },
        index);
    m_Textures[index] = texture;
}

Texture BindlessTextureDescriptorsRingBuffer::GetTexture(u32 index) const
{
    return m_Textures[index];
}

u32 BindlessTextureDescriptorsRingBuffer::GetDefaultTexture(Images::DefaultKind texture) const
{
    return m_DefaultTextures[(u32)texture];
}

u32 BindlessTextureDescriptorsRingBuffer::GetNextIndex(u32 index) const
{
    return (index + 1) % m_MaxBindlessCount;
}

