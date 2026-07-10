#pragma once

#include "Rendering/Buffer/Buffer.h"
#include "Rendering/Image/Image.h"

#include <CoreLib/types.h>
#include <CoreLib/String/StringId.h>

namespace RG
{
enum class ResourceFlags : u8
{
    None = 0,
    /* this resource is imported (external) and cannot be aliased */
    Imported = BIT(0),
    /* this resource cannot be aliased */
    Volatile = BIT(1),
    Split = BIT(2),
    Merge = BIT(3),

    /* if the resource version should be automatically updated to the latest */
    AutoUpdate = BIT(4),
};

CREATE_ENUM_FLAGS_OPERATORS(ResourceFlags)

enum class ResourceCreationFlags : u8
{
    None = 0,
    /* this resource cannot be aliased */
    Volatile = BIT(1),
    /* if the resource version should be automatically updated to the latest */
    AutoUpdate = BIT(4),
};

CREATE_ENUM_FLAGS_OPERATORS(ResourceCreationFlags)

static_assert((u8)ResourceCreationFlags::None == (u8)ResourceFlags::None);
static_assert((u8)ResourceCreationFlags::AutoUpdate == (u8)ResourceFlags::AutoUpdate);
static_assert((u8)ResourceCreationFlags::Volatile == (u8)ResourceFlags::Volatile);

class ResourceHandleBase
{
    friend class Graph;
    friend class GraphWatcher;
    constexpr static u16 INVALID = (u16)~0;

public:
    ResourceHandleBase() = default;
    ResourceHandleBase(const ResourceHandleBase&) = default;
    ResourceHandleBase& operator=(const ResourceHandleBase&) = default;
    ResourceHandleBase(ResourceHandleBase&&) = default;
    ResourceHandleBase& operator=(ResourceHandleBase&&) = default;
    ~ResourceHandleBase() = default;

    auto operator<=>(const ResourceHandleBase&) const = default;
    constexpr bool IsValid() const;

    constexpr ResourceFlags GetFlags() const;
    constexpr void AddFlags(ResourceFlags flag);
    constexpr bool HasFlags(ResourceFlags flag) const;

protected:
    ResourceHandleBase(ResourceFlags flags, u16 index, u16 version);

protected:
    u16 m_Index = (u16)~0;
    u16 m_Version = 0;
    ResourceFlags m_Flags{ResourceFlags::None};
};

class BufferResource : public ResourceHandleBase
{
    friend class Graph;
    friend class GraphWatcher;

public:
    BufferResource() = default;
    BufferResource(const BufferResource&) = default;
    BufferResource& operator=(const BufferResource&) = default;
    BufferResource(BufferResource&&) = default;
    BufferResource& operator=(BufferResource&&) = default;
    ~BufferResource() = default;

    auto operator<=>(const BufferResource&) const = default;
    std::string AsString() const;

private:
    BufferResource(ResourceFlags flags, u16 index, u16 version);
};

class ImageResource : public ResourceHandleBase
{
    friend class Graph;
    friend class GraphWatcher;
    constexpr static u8 NO_EXTRA = (u8)~0;

public:
    ImageResource() = default;
    ImageResource(const ImageResource&) = default;
    ImageResource& operator=(const ImageResource&) = default;
    ImageResource(ImageResource&&) = default;
    ImageResource& operator=(ImageResource&&) = default;
    ~ImageResource() = default;

    auto operator<=>(const ImageResource&) const = default;
    std::string AsString() const;

private:
    ImageResource(ResourceFlags flags, u16 index, u16 version);

private:
    u8 m_Extra{NO_EXTRA};
};

template <typename Type>
class PersistentResource
{
    constexpr static u32 INVALID = ~0u;
    friend class Graph;

public:
    constexpr bool HasValue() const { return m_Index != INVALID; }

private:
    u32 m_Index{INVALID};
};

struct PersistentBufferTag
{
};

struct PersistentImageTag
{
};

using PersistentBufferResource = PersistentResource<PersistentBufferTag>;
using PersistentImageResource = PersistentResource<PersistentImageTag>;

struct RGBufferDescription
{
    u64 SizeBytes{0};
};

enum class RGImageInference : u8
{
    None = 0,
    Size2d = BIT(1),
    Depth = BIT(2),
    Format = BIT(3),
    Kind = BIT(4),
    Filter = BIT(5),
    Views = BIT(6),

    Size = Size2d | Depth,
    Full = Size | Format | Kind | Filter | Views,
};

CREATE_ENUM_FLAGS_OPERATORS(RGImageInference);

struct RGImageDescription
{
    RGImageInference Inference{RGImageInference::None};
    f32 Width{1};
    f32 Height{1};
    f32 LayersDepth{1};
    i8 Mipmaps{1};
    ImageResource Reference{};
    Format Format{Format::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageFilter MipmapFilter{ImageFilter::Linear};
};

struct ResourceBase
{
    static constexpr u32 NO_ACCESS = ~0u;
    u32 FirstAccess{NO_ACCESS};
    u32 LastAccess{NO_ACCESS};

    bool IsImported{false};
    bool IsExported{false};

    StringId Name{};
};

struct RGBuffer : ResourceBase
{
    ::BufferDescription Description{};
    BufferResource AliasedFrom{};
    Buffer Resource{};
};

struct RGImageExtraInfo
{
    u16 Version{0};
    ImageLayout Layout{ImageLayout::Undefined};
};

enum class RGImageState : u8
{
    Merged = 0,
    Split = 1,
    Divergent = 2,
};

struct RGImage : ResourceBase
{
    ::ImageDescription Description{};
    ImageResource AliasedFrom{};
    Image Resource{};
    ImageLayout Layout{ImageLayout::Undefined};
    u16 LatestVersion{0};
    u16 ActiveSplitCount{0};
    RGImageState State{RGImageState::Merged};
    std::vector<RGImageExtraInfo> Extras{};
};

constexpr bool ResourceHandleBase::IsValid() const
{
    return m_Index != INVALID;
}

constexpr ResourceFlags ResourceHandleBase::GetFlags() const
{
    return m_Flags;
}

constexpr void ResourceHandleBase::AddFlags(ResourceFlags flag)
{
    m_Flags |= flag;
}

constexpr bool ResourceHandleBase::HasFlags(ResourceFlags flag) const
{
    return enumHasAny(m_Flags, flag);
}

inline ResourceHandleBase::ResourceHandleBase(ResourceFlags flags, u16 index, u16 version)
    : m_Index(index), m_Version(version), m_Flags(flags)
{
}

inline std::string BufferResource::AsString() const
{
    return !IsValid() ? "Invalid" : std::format("Buffer ({}.{})", m_Index, m_Version);
}

inline BufferResource::BufferResource(ResourceFlags flags, u16 index, u16 version)
    : ResourceHandleBase(flags, index, version)
{
}

inline std::string ImageResource::AsString() const
{
    return !IsValid() ? "Invalid" : std::format("Image ({}.{})", m_Index, m_Version);
}

inline ImageResource::ImageResource(ResourceFlags flags, u16 index, u16 version)
    : ResourceHandleBase(flags, index, version)
{
}
}

namespace std
{
template <>
struct formatter<RG::BufferResource>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(RG::BufferResource resource, format_context& ctx) const
    {
        return format_to(ctx.out(), "{}", resource.AsString());
    }
};

template <>
struct formatter<RG::ImageResource>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(RG::ImageResource resource, format_context& ctx) const
    {
        return format_to(ctx.out(), "{}", resource.AsString());
    }
};
}
