#include "BindlessTextureDescriptorsRingBuffer.h"

BindlessTextureDescriptorsRingBuffer::BindlessTextureDescriptorsRingBuffer(u32 maxCount, const ShaderPipelineTemplate* pipelineTemplate)
    : m_MaxBindlessCount(maxCount)
{
    m_BindlessDescriptorSet = ShaderDescriptors::Builder()
        .SetTemplate(pipelineTemplate, DescriptorAllocatorKind::Resources)
        // todo: make this (2) an enum
        .ExtractSet(2)
        .BindlessCount(maxCount)
        .Build();
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
    const ShaderDescriptors::BindingInfo bindingInfo = m_BindlessDescriptorSet.GetBindingInfo(UNIFORM_TEXTURES);

    m_BindlessDescriptorSet.UpdateGlobalBinding(
        bindingInfo,
        texture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly),
        m_Tail);

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

