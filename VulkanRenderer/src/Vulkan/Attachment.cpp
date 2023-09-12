#include "Attachment.h"

AttachmentTemplate AttachmentTemplate::Builder::Build()
{
    return AttachmentTemplate::Create(m_CreateInfo);   
}

AttachmentTemplate::Builder& AttachmentTemplate::Builder::PresentationDefaults()
{
    CreateInfo createInfo = {};

    createInfo.InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.FinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    createInfo.OnLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
    createInfo.OnStore = VK_ATTACHMENT_STORE_OP_STORE;
    createInfo.Samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.ReferenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    createInfo.Type = AttachmentType::Presentation;
    
    m_CreateInfo = createInfo;
    
    return *this;
}

AttachmentTemplate::Builder& AttachmentTemplate::Builder::ColorDefaults()
{
    CreateInfo createInfo = {};

    createInfo.InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    createInfo.OnLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
    createInfo.OnStore = VK_ATTACHMENT_STORE_OP_STORE;
    createInfo.Samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.ReferenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    createInfo.Type = AttachmentType::Color;

    m_CreateInfo = createInfo;
    
    return *this;
}

AttachmentTemplate::Builder& AttachmentTemplate::Builder::DepthDefaults()
{
    CreateInfo createInfo = {};

    createInfo.InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.FinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    createInfo.OnLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
    createInfo.OnStore = VK_ATTACHMENT_STORE_OP_STORE;
    createInfo.Samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.ReferenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    createInfo.Type = AttachmentType::DepthStencil;
    
    m_CreateInfo = createInfo;
    
    return *this;
}

AttachmentTemplate::Builder& AttachmentTemplate::Builder::SetFormat(VkFormat format)
{
    m_CreateInfo.Format = format;
    
    return *this;
}

AttachmentTemplate AttachmentTemplate::Create(const Builder::CreateInfo& createInfo)
{
    AttachmentTemplate attachment = {};
    
    VkAttachmentDescription attachmentDescription = {};
    attachmentDescription.format = createInfo.Format;
    attachmentDescription.samples = createInfo.Samples;
    attachmentDescription.initialLayout = createInfo.InitialLayout;
    attachmentDescription.finalLayout = createInfo.FinalLayout; 
    attachmentDescription.loadOp = createInfo.OnLoad;
    attachmentDescription.storeOp = createInfo.OnStore;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachment.m_Type = createInfo.Type;
    attachment.m_AttachmentDescription = attachmentDescription;
    attachment.m_AttachmentReferenceLayout = createInfo.ReferenceLayout;

    return attachment;
}

Attachment Attachment::Builder::Build()
{
    return Attachment::Create(m_CreateInfo);
}

Attachment::Builder& Attachment::Builder::SetType(AttachmentType type)
{
    m_CreateInfo.Type = type;

    return *this;
}

Attachment::Builder& Attachment::Builder::FromImageData(const ImageData& imageData)
{
    m_CreateInfo.ImageData = imageData;

    return *this;
}

Attachment Attachment::Create(const Builder::CreateInfo& createInfo)
{
    Attachment attachment = {};

    attachment.m_Type = createInfo.Type;
    attachment.m_ImageData = createInfo.ImageData;

    return attachment;
}
