#include "RenderingInfo.h"

#include "Vulkan/Driver.h"

RenderingAttachment RenderingAttachment::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

RenderingAttachment RenderingAttachment::Builder::Build(DeletionQueue& deletionQueue)
{
    RenderingAttachment attachment = RenderingAttachment::Create(m_CreateInfo);
    deletionQueue.AddDeleter([attachment](){ RenderingAttachment::Destroy(attachment); });

    return attachment;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::SetType(RenderingAttachmentType type)
{
    m_CreateInfo.Type = type;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::FromImage(const Image& image, ImageLayout imageLayout)
{
    m_CreateInfo.Image = &image;
    m_CreateInfo.Layout = imageLayout;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::LoadStoreOperations(AttachmentLoad onLoad,
    AttachmentStore onStore)
{
    m_CreateInfo.OnLoad = onLoad;
    m_CreateInfo.OnStore = onStore;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::vec4& value)
{
    m_CreateInfo.ClearValue.Color.F = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::uvec4& value)
{
    m_CreateInfo.ClearValue.Color.U = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::ivec4& value)
{
    m_CreateInfo.ClearValue.Color.I = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(f32 depth)
{
    m_CreateInfo.ClearValue.DepthStencil = {.Depth = depth, .Stencil = 0};

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(f32 depth, u32 stencil)
{
    m_CreateInfo.ClearValue.DepthStencil = {.Depth = depth, .Stencil = stencil};

    return *this;
}

RenderingAttachment RenderingAttachment::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void RenderingAttachment::Destroy(const RenderingAttachment& renderingAttachment)
{
    Driver::Destroy(renderingAttachment);
}

RenderingInfo RenderingInfo::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

RenderingInfo RenderingInfo::Builder::Build(DeletionQueue& deletionQueue)
{
    RenderingInfo renderingInfo = RenderingInfo::Create(m_CreateInfo);
    deletionQueue.AddDeleter([renderingInfo]() { RenderingInfo::Destroy(renderingInfo); });

    return renderingInfo;
}

RenderingInfo::Builder& RenderingInfo::Builder::AddAttachment(const RenderingAttachment& attachment)
{
    ASSERT(attachment.m_Type == RenderingAttachmentType::Color ||
        attachment.m_Type == RenderingAttachmentType::Depth, "Unsupported attachment type")
    
    if (attachment.m_Type == RenderingAttachmentType::Color)
        m_CreateInfo.ColorAttachments.push_back(attachment);
    else
        m_CreateInfo.DepthAttachment = attachment;

    return *this;
}

RenderingInfo::Builder& RenderingInfo::Builder::SetRenderArea(const glm::uvec2& area)
{
    m_CreateInfo.RenderArea = area;

    return *this;
}

RenderingInfo RenderingInfo::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void RenderingInfo::Destroy(const RenderingInfo& renderingInfo)
{
    Driver::Destroy(renderingInfo);
}