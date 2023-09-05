#pragma once

#include "types.h"
#include "VulkanCommon.h"

#include <vector>
#include <vulkan/vulkan_core.h>

class RenderPass;
class Attachment;

class Framebuffer
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Framebuffer;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkDevice Device;
            VkRenderPass RenderPass;
            std::vector<VkImageView> Attachments;
            u32 Width;
            u32 Height;
        };
    public:
        Framebuffer Build();
        Builder& SetRenderPass(const RenderPass& renderPass);
        Builder& AddAttachment(const Attachment& attachment);
        Builder& SetAttachments(const std::vector<Attachment>& attachments);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Framebuffer Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Framebuffer& framebuffer);
private:
    VkFramebuffer m_Framebuffer{VK_NULL_HANDLE};
    VkExtent2D m_Extent{};
    VkDevice m_Device{VK_NULL_HANDLE};
};
