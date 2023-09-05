#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanCommon.h"

enum class AttachmentType {Presentation, Color, DepthStencil};

class AttachmentTemplate
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class AttachmentTemplate;
        struct CreateInfo
        {
            VkFormat Format;
            VkSampleCountFlagBits Samples;
            VkAttachmentLoadOp OnLoad;
            VkAttachmentStoreOp OnStore;
            VkImageLayout InitialLayout;
            VkImageLayout FinalLayout;
            VkImageLayout ReferenceLayout;
            AttachmentType Type;
        };
    public:
        AttachmentTemplate Build();
        Builder& PresentationDefaults();
        Builder& ColorDefaults();
        Builder& DepthDefaults();
        Builder& SetFormat(VkFormat format);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static AttachmentTemplate Create(const Builder::CreateInfo& createInfo);
private:
    VkAttachmentDescription m_AttachmentDescription{};
    VkImageLayout m_AttachmentReferenceLayout{};
    AttachmentType m_Type{};
};

class Attachment
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Attachment;
        struct CreateInfo
        {
            ImageData ImageData;
            AttachmentType Type;
        };
    public:
        Attachment Build();
        Builder& SetType(AttachmentType type);
        Builder& FromImageData(const ImageData& imageData);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Attachment Create(const Builder::CreateInfo& createInfo);
private:
    ImageData m_ImageData{};
    AttachmentType m_Type{};
};