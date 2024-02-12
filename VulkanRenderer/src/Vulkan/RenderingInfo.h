#pragma once
#include <optional>

#include "RenderCommand.h"

enum class RenderingAttachmentType {Color, Depth};

class RenderingAttachment
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        FRIEND_INTERNAL
        friend class RenderingAttachment;
        struct CreateInfo
        {
            VkRenderingAttachmentInfo AttachmentInfo;
            RenderingAttachmentType Type;
        };
    public:
        RenderingAttachment Build();
        Builder& SetType(RenderingAttachmentType type);
        Builder& FromImage(const Image& image, ImageLayout imageLayout);
        Builder& LoadStoreOperations(VkAttachmentLoadOp onLoad, VkAttachmentStoreOp onStore);
        Builder& ClearValue(VkClearValue clearValue);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static RenderingAttachment Create(const Builder::CreateInfo& createInfo);
private:
    VkRenderingAttachmentInfo m_AttachmentInfo{};
    RenderingAttachmentType m_Type{};
};

class RenderingInfo
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        FRIEND_INTERNAL
        friend class RenderingInfo;
        struct CreateInfo
        {
            std::vector<VkRenderingAttachmentInfo> ColorAttachments;
            std::optional<VkRenderingAttachmentInfo> DepthAttachment;
            VkRect2D RenderArea;
        };
    public:
        RenderingInfo Build();
        Builder& AddAttachment(const RenderingAttachment& attachment);
        Builder& SetRenderArea(const glm::uvec2& area);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static RenderingInfo Create(const Builder::CreateInfo& createInfo);
private:
    VkRenderingInfo m_RenderingInfo{};
    std::vector<VkRenderingAttachmentInfo> m_ColorAttachments;
    std::optional<VkRenderingAttachmentInfo> m_DepthAttachment;
};
