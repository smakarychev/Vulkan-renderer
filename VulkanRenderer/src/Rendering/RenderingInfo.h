#pragma once

#include "DriverResourceHandle.h"
#include "ImageTraits.h"
#include "RenderingCommon.h"

#include <optional>

class DeletionQueue;

enum class RenderingAttachmentType {Color, Depth};

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
            
            RenderingAttachmentType Type;
            ClearValue ClearValue{};
            const Image* Image{nullptr};
            ImageLayout Layout;
            AttachmentLoad OnLoad;
            AttachmentStore OnStore;
        };
    public:
        RenderingAttachment Build();
        RenderingAttachment Build(DeletionQueue& deletionQueue);
        Builder& SetType(RenderingAttachmentType type);
        Builder& FromImage(const Image& image, ImageLayout imageLayout);
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
    ResourceHandle<RenderingAttachment> Handle() const { return m_ResourceHandle; }
private:
    RenderingAttachmentType m_Type{};
    ResourceHandle<RenderingAttachment> m_ResourceHandle{};
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
        Builder& SetRenderArea(const glm::uvec2& area);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static RenderingInfo Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const RenderingInfo& renderingInfo);
private:
    ResourceHandle<RenderingInfo> Handle() const { return m_ResourceHandle; }
private:
    glm::uvec2 m_RenderArea;
    ResourceHandle<RenderingInfo> m_ResourceHandle{};
};
