#pragma once

#include "ResourceHandle.h"
#include "Image/ImageTraits.h"
#include "Image/Image.h"

#include <optional>

class DeletionQueue;

enum class RenderingAttachmentType : u8 {Color, DepthStencil};

struct ColorClearValue
{
    union
    {
        glm::uvec4 U{};
        glm::fvec4 F;
        glm::ivec4 I;
    };
};

struct DepthStencilClearValue
{
    f32 Depth{};
    u32 Stencil{};
};

struct ClearValue
{
    union
    {
        ColorClearValue Color{};
        DepthStencilClearValue DepthStencil;
    };
};

struct ColorAttachmentDescription
{
    ImageSubresourceDescription Subresource{};
    AttachmentLoad OnLoad{AttachmentLoad::Load};
    AttachmentStore OnStore{AttachmentStore::Store};
    ColorClearValue ClearColor{};
};

struct DepthStencilAttachmentDescription
{
    ImageSubresourceDescription Subresource{};
    AttachmentLoad OnLoad{AttachmentLoad::Load};
    AttachmentStore OnStore{AttachmentStore::Store};
    DepthStencilClearValue ClearDepthStencil{};
};

struct RenderingAttachmentDescription
{
    ImageSubresourceDescription Subresource{};
    AttachmentLoad OnLoad{AttachmentLoad::Unspecified};
    AttachmentStore OnStore{AttachmentStore::Unspecified};
    ClearValue Clear{};

    RenderingAttachmentDescription() = default;
    RenderingAttachmentDescription(const ColorAttachmentDescription& description)
        :
        Subresource(description.Subresource),
        OnLoad(description.OnLoad),
        OnStore(description.OnStore),
        Clear{.Color = description.ClearColor} {}
    RenderingAttachmentDescription(const DepthStencilAttachmentDescription& description)
        :
        Subresource(description.Subresource),
        OnLoad(description.OnLoad),
        OnStore(description.OnStore),
        Clear{.DepthStencil = description.ClearDepthStencil} {}
};

struct RenderingAttachmentCreateInfo
{
    RenderingAttachmentDescription Description{};
    Image Image{};
    ImageLayout Layout{ImageLayout::Undefined};
};

struct RenderingAttachmentTag{};
using RenderingAttachment = ResourceHandleType<RenderingAttachmentTag>;

struct RenderingInfoCreateInfo
{
    glm::uvec2 RenderArea{0};
    Span<const RenderingAttachment> ColorAttachments{};
    std::optional<RenderingAttachment> DepthAttachment{};
};

struct RenderingInfoTag{};
using RenderingInfo = ResourceHandleType<RenderingInfoTag>;