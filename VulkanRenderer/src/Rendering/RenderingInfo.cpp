#include "RenderingInfo.h"

#include "Vulkan/Driver.h"

RenderingAttachment::Builder::Builder(const RenderingAttachmentDescription& description)
{
    m_CreateInfo.Description = description;
}

RenderingAttachment::Builder::Builder(const ColorAttachmentDescription& description)
{
    m_CreateInfo.Description = {
        .Subresource = description.Subresource,
        .Type = RenderingAttachmentType::Color,
        .Clear = RenderingAttachmentDescription::ClearValue{.Color = {.F = description.ClearColor.F}},
        .OnLoad = description.OnLoad,
        .OnStore = description.OnStore};
}

RenderingAttachment::Builder::Builder(const DepthStencilAttachmentDescription& description)
{
    m_CreateInfo.Description = {
        .Subresource = description.Subresource,
        .Type = RenderingAttachmentType::Depth,
        .Clear = RenderingAttachmentDescription::ClearValue{.DepthStencil =
            {.Depth = description.ClearDepth, .Stencil = description.ClearStencil}},
        .OnLoad = description.OnLoad,
        .OnStore = description.OnStore};
}

RenderingAttachment RenderingAttachment::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

RenderingAttachment RenderingAttachment::Builder::Build(DeletionQueue& deletionQueue)
{
    RenderingAttachment attachment = RenderingAttachment::Create(m_CreateInfo);
    deletionQueue.Enqueue(attachment);

    return attachment;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::SetType(RenderingAttachmentType type)
{
    m_CreateInfo.Description.Type = type;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::FromImage(const Image& image, ImageLayout imageLayout)
{
    m_CreateInfo.Image = &image;
    m_CreateInfo.Layout = imageLayout;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::View(ImageSubresourceDescription::Packed subresource)
{
    m_CreateInfo.Description.Subresource = subresource;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::LoadStoreOperations(AttachmentLoad onLoad,
    AttachmentStore onStore)
{
    m_CreateInfo.Description.OnLoad = onLoad;
    m_CreateInfo.Description.OnStore = onStore;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::vec4& value)
{
    m_CreateInfo.Description.Clear.Color.F = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::uvec4& value)
{
    m_CreateInfo.Description.Clear.Color.U = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(const glm::ivec4& value)
{
    m_CreateInfo.Description.Clear.Color.I = value;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(f32 depth)
{
    m_CreateInfo.Description.Clear.DepthStencil = {.Depth = depth, .Stencil = 0};

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(f32 depth, u32 stencil)
{
    m_CreateInfo.Description.Clear.DepthStencil = {.Depth = depth, .Stencil = stencil};

    return *this;
}

RenderingAttachment RenderingAttachment::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void RenderingAttachment::Destroy(const RenderingAttachment& renderingAttachment)
{
    Driver::Destroy(renderingAttachment.Handle());
}

RenderingInfo RenderingInfo::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

RenderingInfo RenderingInfo::Builder::Build(DeletionQueue& deletionQueue)
{
    RenderingInfo renderingInfo = RenderingInfo::Create(m_CreateInfo);
    deletionQueue.Enqueue(renderingInfo);

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

RenderingInfo::Builder& RenderingInfo::Builder::SetResolution(const glm::uvec2& resolution)
{
    m_CreateInfo.RenderArea = resolution;

    return *this;
}

RenderingInfo RenderingInfo::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void RenderingInfo::Destroy(const RenderingInfo& renderingInfo)
{
    Driver::Destroy(renderingInfo.Handle());
}