﻿#include "Framebuffer.h"

#include "Core/core.h"
#include "Driver.h"
#include "VulkanCore.h"

Framebuffer Framebuffer::Builder::Build()
{
    ASSERT(!m_CreateInfo.Attachments.empty(), "Have to provide at least one attachment")
    Framebuffer framebuffer = Framebuffer::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([framebuffer](){ Framebuffer::Destroy(framebuffer); });

    return framebuffer;
}

Framebuffer Framebuffer::Builder::BuildManualLifetime()
{
    ASSERT(!m_CreateInfo.Attachments.empty(), "Have to provide at least one attachment")
    return Framebuffer::Create(m_CreateInfo);
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

    VulkanCheck(vkCreateFramebuffer(Driver::DeviceHandle(), &framebufferCreateInfo, nullptr, &framebuffer.m_Framebuffer),
        "Failed to create framebuffer");
    framebuffer.m_Extent = {.width = createInfo.Width, .height = createInfo.Height};
    
    return framebuffer;
}

void Framebuffer::Destroy(const Framebuffer& framebuffer)
{
    vkDestroyFramebuffer(Driver::DeviceHandle(), framebuffer.m_Framebuffer, nullptr);
}