#pragma once

#include "types.h"
#include "VulkanCommon.h"

#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

class CommandBuffer;
class Framebuffer;
class AttachmentTemplate;
class Swapchain;
class Device;

class Subpass
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Subpass;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<VkAttachmentDescription> Attachments;
            std::vector<VkAttachmentReference> ColorReferences;
            std::optional<VkAttachmentReference> DepthStencilReference;
        };
    public:
        Subpass Build();
        Builder& AddAttachment(const AttachmentTemplate& attachment);
        Builder& SetAttachments(const std::vector<AttachmentTemplate>& attachments);
    private:
        CreateInfo m_CreateInfo;
    };
    static Subpass Create(const Builder::CreateInfo& createInfo);
private:
    std::vector<VkAttachmentDescription> m_Attachments;
    std::vector<VkAttachmentReference> m_ColorReferences;
    std::optional<VkAttachmentReference> m_DepthStencilReference{};
};

struct SubpassDependencyInfo
{
    VkPipelineStageFlags SourceStage;
    VkPipelineStageFlags DestinationStage;
    VkAccessFlags SourceAccessMask;
    VkAccessFlags DestinationAccessMask;
};

class RenderPass
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class RenderPass;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<VkSubpassDescription> Subpasses;
            std::vector<VkSubpassDependency> SubpassDependencies;
            std::vector<VkAttachmentDescription> Attachments;
            VkDevice Device;
        };
    public:
        RenderPass Build();
        Builder& SetDevice(const Device& device);
        Builder& AddSubpass(const Subpass& subpass);
        Builder& AddSubpassDependency(u32 source, const Subpass& destination, const SubpassDependencyInfo& dependencyInfo);
    private:
        void FinishSubpasses();
    private:
        CreateInfo m_CreateInfo;
        std::vector<const Subpass*> m_Subpasses;
    };
public:
    static RenderPass Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const RenderPass& renderPass);

    void Begin(const CommandBuffer& commandBuffer, const Framebuffer& framebuffer, const std::vector<VkClearValue>& clearValues);
    void End(const CommandBuffer& commandBuffer);
private:
    VkRenderPass m_RenderPass{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
};
