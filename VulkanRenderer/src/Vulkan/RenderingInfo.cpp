#include "RenderingInfo.h"

#include "Driver.h"

RenderingAttachment RenderingAttachment::Builder::Build()
{
    return RenderingAttachment::Create(m_CreateInfo);
}

RenderingAttachment::Builder& RenderingAttachment::Builder::SetType(RenderingAttachmentType type)
{
    m_CreateInfo.Type = type;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::FromImage(const ImageData& imageData,
    VkImageLayout imageLayout)
{
    m_CreateInfo.AttachmentInfo.imageView = imageData.View;
    m_CreateInfo.AttachmentInfo.imageLayout = imageLayout;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::LoadStoreOperations(VkAttachmentLoadOp onLoad,
    VkAttachmentStoreOp onStore)
{
    m_CreateInfo.AttachmentInfo.loadOp = onLoad;
    m_CreateInfo.AttachmentInfo.storeOp = onStore;

    return *this;
}

RenderingAttachment::Builder& RenderingAttachment::Builder::ClearValue(VkClearValue clearValue)
{
    m_CreateInfo.AttachmentInfo.clearValue = clearValue;

    return *this;
}

RenderingAttachment RenderingAttachment::Create(const Builder::CreateInfo& createInfo)
{
    RenderingAttachment renderingAttachment = {};

    renderingAttachment.m_Type = createInfo.Type;

    renderingAttachment.m_AttachmentInfo = {};
    renderingAttachment.m_AttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderingAttachment.m_AttachmentInfo.clearValue = createInfo.AttachmentInfo.clearValue;
    renderingAttachment.m_AttachmentInfo.imageLayout = createInfo.AttachmentInfo.imageLayout;
    renderingAttachment.m_AttachmentInfo.imageView = createInfo.AttachmentInfo.imageView;
    renderingAttachment.m_AttachmentInfo.loadOp = createInfo.AttachmentInfo.loadOp;
    renderingAttachment.m_AttachmentInfo.storeOp = createInfo.AttachmentInfo.storeOp;
    renderingAttachment.m_AttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;

    return renderingAttachment;
}

RenderingInfo RenderingInfo::Builder::Build()
{
    return RenderingInfo::Create(m_CreateInfo);
}

RenderingInfo::Builder& RenderingInfo::Builder::AddAttachment(const RenderingAttachment& attachment)
{
    Driver::Unpack(attachment, m_CreateInfo);

    return *this;
}

RenderingInfo::Builder& RenderingInfo::Builder::SetRenderArea(const glm::uvec2& area)
{
    m_CreateInfo.RenderArea = {.offset = {0, 0}, .extent = {area.x, area.y}};

    return *this;
}

RenderingInfo RenderingInfo::Create(const Builder::CreateInfo& createInfo)
{
    RenderingInfo renderingInfo = {};

    renderingInfo.m_ColorAttachments = createInfo.ColorAttachments;
    renderingInfo.m_DepthAttachment = createInfo.DepthAttachment;
    
    renderingInfo.m_RenderingInfo = {};
    
    renderingInfo.m_RenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.m_RenderingInfo.layerCount = 1;
    renderingInfo.m_RenderingInfo.renderArea = createInfo.RenderArea;
    renderingInfo.m_RenderingInfo.colorAttachmentCount = (u32)renderingInfo.m_ColorAttachments.size();
    renderingInfo.m_RenderingInfo.pColorAttachments = renderingInfo.m_ColorAttachments.data();
    if (createInfo.DepthAttachment.has_value())
        renderingInfo.m_RenderingInfo.pDepthAttachment = renderingInfo.m_DepthAttachment.operator->();

    return renderingInfo;
}
