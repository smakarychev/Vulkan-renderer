#include "Framebuffer.h"

#include "core.h"
#include "Driver.h"

Framebuffer Framebuffer::Builder::Build()
{
    ASSERT(!m_CreateInfo.Attachments.empty(), "Have to provide at least one attachment")
    Framebuffer framebuffer = Framebuffer::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([framebuffer](){ Framebuffer::Destroy(framebuffer); });

    return framebuffer;
}

Framebuffer::Builder& Framebuffer::Builder::SetRenderPass(const RenderPass& renderPass)
{
    Driver::Unpack(renderPass, m_CreateInfo);
    
    return *this;
}

Framebuffer::Builder& Framebuffer::Builder::AddAttachment(const Attachment& attachment)
{
    Driver::Unpack(attachment, m_CreateInfo);
        
    return *this;
}

Framebuffer::Builder& Framebuffer::Builder::SetAttachments(const std::vector<Attachment>& attachments)
{
    for (auto& attachment : attachments)
        Driver::Unpack(attachment, m_CreateInfo);
    
    return *this;
}

Framebuffer Framebuffer::Create(const Builder::CreateInfo& createInfo)
{
    Framebuffer framebuffer = {};
    
    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = createInfo.RenderPass;
    framebufferCreateInfo.attachmentCount = (u32)createInfo.Attachments.size();
    framebufferCreateInfo.pAttachments = createInfo.Attachments.data();
    framebufferCreateInfo.width = createInfo.Width;
    framebufferCreateInfo.height = createInfo.Height;
    framebufferCreateInfo.layers = 1;

    VulkanCheck(vkCreateFramebuffer(createInfo.Device, &framebufferCreateInfo, nullptr, &framebuffer.m_Framebuffer),
        "Failed to create framebuffer");
    framebuffer.m_Device = createInfo.Device;
    framebuffer.m_Extent = {.width = createInfo.Width, .height = createInfo.Height};
    
    return framebuffer;
}

void Framebuffer::Destroy(const Framebuffer& framebuffer)
{
    vkDestroyFramebuffer(framebuffer.m_Device, framebuffer.m_Framebuffer, nullptr);
}
