#include "BindlessTextureDescriptorsRingBuffer.h"

#include "RenderGraph/Passes/Generated/MaterialsBindGroup.generated.h"

BindlessTextureDescriptorsRingBuffer::BindlessTextureDescriptorsRingBuffer(u32 maxCount, const Shader& shader)
    : m_MaxBindlessCount(maxCount), m_MaterialsShader(&shader)
{
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

u32 BindlessTextureDescriptorsRingBuffer::AddTexture(const Texture& texture)
{
    MaterialsShaderBindGroup bindGroup(*m_MaterialsShader);
    bindGroup.SetTexturesGlobally(texture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly), m_Tail);

    const u32 toReturn = m_Tail;
    
    if (WillOverflow())
        m_Head = GetNextIndex(m_Head);

    m_Tail = GetNextIndex(m_Tail);

    return toReturn;
}

u32 BindlessTextureDescriptorsRingBuffer::GetNextIndex(u32 index) const
{
    return (index + 1) % m_MaxBindlessCount;
}

