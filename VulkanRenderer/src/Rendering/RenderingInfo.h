#pragma once

#include "ResourceHandle.h"
#include "Image/ImageTraits.h"
#include "Image/Image.h"
#include "RenderingCommon.h"

#include <optional>

class DeletionQueue;

enum class RenderingAttachmentType {Color, Depth};

struct RenderingAttachmentDescription
{
    union ClearValue
    {
        union Color
        {
            glm::fvec4 F;
            glm::uvec4 U;
            glm::ivec4 I;
        };
        struct DepthStencil
        {
            f32 Depth;
            u32 Stencil;
        };
        Color Color;
        DepthStencil DepthStencil;
    };

    ImageSubresourceDescription Subresource{};
    RenderingAttachmentType Type;
    ClearValue Clear{};
    AttachmentLoad OnLoad{AttachmentLoad::Unspecified};
    AttachmentStore OnStore{AttachmentStore::Unspecified};
};

struct ColorAttachmentDescription
{
    union Color
    {
        glm::fvec4 F;
        glm::uvec4 U;
        glm::ivec4 I;
    };
    ImageSubresourceDescription Subresource{};
    AttachmentLoad OnLoad{AttachmentLoad::Load};
    AttachmentStore OnStore{AttachmentStore::Store};
    Color ClearColor{};
};

struct DepthStencilAttachmentDescription
{
    ImageSubresourceDescription Subresource{};
    AttachmentLoad OnLoad{AttachmentLoad::Load};
    AttachmentStore OnStore{AttachmentStore::Store};
    f32 ClearDepth{};
    u32 ClearStencil{};
};

class RenderingAttachment
{
    friend class RenderingInfo;
    FRIEND_INTERNAL
public:
    class Builder
    {
        FRIEND_INTERNAL
        friend class RenderingAttachment;
        struct CreateInfo
        {
            RenderingAttachmentDescription Description{};
            const Image* Image{nullptr};
            ImageLayout Layout;
        };
    public:
        Builder() = default;
        Builder(const RenderingAttachmentDescription& description);
        Builder(const ColorAttachmentDescription& description);
        Builder(const DepthStencilAttachmentDescription& description);
        RenderingAttachment Build();
        RenderingAttachment Build(DeletionQueue& deletionQueue);
        Builder& SetType(RenderingAttachmentType type);
        Builder& FromImage(const Image& image, ImageLayout imageLayout);
        Builder& View(ImageSubresourceDescription subresource);
        Builder& LoadStoreOperations(AttachmentLoad onLoad, AttachmentStore onStore);
        Builder& ClearValue(const glm::vec4& value);
        Builder& ClearValue(const glm::uvec4& value);
        Builder& ClearValue(const glm::ivec4& value);
        Builder& ClearValue(f32 depth);
        Builder& ClearValue(f32 depth, u32 stencil);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static RenderingAttachment Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const RenderingAttachment& renderingAttachment);
private:
    ResourceHandleType<RenderingAttachment> Handle() const { return m_ResourceHandle; }
private:
    RenderingAttachmentType m_Type{};
    ResourceHandleType<RenderingAttachment> m_ResourceHandle{};
};

class RenderingInfo
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class RenderingInfo;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            glm::uvec2 RenderArea;
            std::vector<RenderingAttachment> ColorAttachments;
            std::optional<RenderingAttachment> DepthAttachment;
        };
    public:
        RenderingInfo Build();
        RenderingInfo Build(DeletionQueue& deletionQueue);
        Builder& AddAttachment(const RenderingAttachment& attachment);
        Builder& SetResolution(const glm::uvec2& resolution);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static RenderingInfo Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const RenderingInfo& renderingInfo);
private:
    ResourceHandleType<RenderingInfo> Handle() const { return m_ResourceHandle; }
private:
    glm::uvec2 m_RenderArea;
    ResourceHandleType<RenderingInfo> m_ResourceHandle{};
};
