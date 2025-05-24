#pragma once

#include "types.h"
#include "Rendering/Buffer/Buffer.h"
#include "Rendering/Image/Image.h"
#include "String/StringId.h"

namespace RG
{
    enum class ResourceFlags : u8
    {
        None = 0,
        Buffer      = BIT(0),
        Image       = BIT(1),
        /* this resource is imported (external) and cannot be aliased */
        Imported    = BIT(2),
        /* this resource cannot be aliased */
        Volatile    = BIT(3),
        Split       = BIT(4),
        Merge       = BIT(5),

        /* if the resource version should be automatically updated to the latest */
        AutoUpdate  = BIT(6),
    };
    CREATE_ENUM_FLAGS_OPERATORS(ResourceFlags)

    enum class ResourceCreationFlags : u8
    {
        None = 0,
        /* this resource cannot be aliased */
        Volatile    = BIT(3),
        /* if the resource version should be automatically updated to the latest */
        AutoUpdate  = BIT(6),
    };
    CREATE_ENUM_FLAGS_OPERATORS(ResourceCreationFlags)

    static_assert((u8)ResourceCreationFlags::None == (u8)ResourceFlags::None);
    static_assert((u8)ResourceCreationFlags::AutoUpdate == (u8)ResourceFlags::AutoUpdate);
    static_assert((u8)ResourceCreationFlags::Volatile == (u8)ResourceFlags::Volatile);
    
    class Resource
    {
        friend class Graph;
        friend class GraphWatcher;
        constexpr static u16 INVALID = (u16)~0;
        constexpr static u8 NO_EXTRA = (u8)~0;
    public:
        Resource() = default;
        Resource(const Resource&) = default;
        Resource& operator=(const Resource&) = default;
        Resource(Resource&&) = default;
        Resource& operator=(Resource&&) = default;
        ~Resource() = default;
            
        auto operator<=>(const Resource&) const = default;
        constexpr bool IsValid() const;

        constexpr ResourceFlags GetFlags() const;
        constexpr void AddFlags(ResourceFlags flag);
        constexpr bool HasFlags(ResourceFlags flag) const;
        constexpr bool IsBuffer() const;
        constexpr bool IsImage() const;

        std::string AsString() const;
    private:
        constexpr Resource(ResourceFlags flagsType, u16 index, u16 version);
        constexpr static Resource Buffer(u16 index, u16 version);
        constexpr static Resource Image(u16 index, u16 version);
    private:
        u16 m_Index = (u16)~0;
        u16 m_Version = 0;
        ResourceFlags m_Flags{ResourceFlags::None};
        u8 m_Extra{NO_EXTRA};
    };

    struct RGBufferDescription
    {
        u64 SizeBytes{0};
    };
    enum class RGImageInference : u8
    {
        None = 0,
        Size2d  = BIT(1),
        Depth   = BIT(2),
        Format  = BIT(3),
        Kind    = BIT(4),
        Filter  = BIT(5),
        Views   = BIT(6),

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
        Resource Reference{};
        Format Format{Format::Undefined};
        ImageKind Kind{ImageKind::Image2d};
        ImageFilter MipmapFilter{ImageFilter::Linear};
    };
    
    
    struct ResourceBase
    {
        static constexpr u32 NO_ACCESS = ~0u;
        u32 FirstAccess{NO_ACCESS};
        u32 LastAccess{NO_ACCESS};

        Resource AliasedFrom{};
        bool IsImported{false};
        bool IsExported{false};

        StringId Name{};
    };
    struct BufferResource : ResourceBase
    {
        ::BufferDescription Description{};
        Buffer Resource{};
    };
    
    struct ImageResourceExtraInfo
    {
        u16 Version{0};
        ImageLayout Layout{ImageLayout::Undefined};
    };
    enum class ImageResourceState : u8
    {
        Merged = 0,
        Split = 1,
        MaybeDivergent = 2,
    };
    struct ImageResource : ResourceBase
    {
        ::ImageDescription Description{};
        Image Resource{};
        ImageLayout Layout{ImageLayout::Undefined};
        u16 LatestVersion{0};
        ImageResourceState State{ImageResourceState::Merged};
        std::vector<ImageResourceExtraInfo> Extras{};
    };

    inline std::string Resource::AsString() const
    {
        if (!IsValid())
            return "Invalid";

        if (IsBuffer())
            return std::format("Buffer ({}.{})", m_Index, m_Version);
        if (IsImage())
            return std::format("Image ({}.{})", m_Index, m_Version);
        return "Invalid";
    }

    constexpr Resource::Resource(ResourceFlags flagsType, u16 index, u16 version)
        : m_Index(index), m_Version(version), m_Flags(flagsType)
    {
    }

    constexpr Resource Resource::Buffer(u16 index, u16 version)
    {
        return Resource(ResourceFlags::Buffer, index, version);
    }

    constexpr Resource Resource::Image(u16 index, u16 version)
    {
        /* image is merged by default */
        return Resource(ResourceFlags::Image | ResourceFlags::Merge, index, version);
    }

    constexpr bool Resource::IsValid() const
    {
        return m_Index != INVALID;
    }

    constexpr ResourceFlags Resource::GetFlags() const
    {
        return m_Flags;
    }

    constexpr void Resource::AddFlags(ResourceFlags flag)
    {
        m_Flags |= flag;
    }

    constexpr bool Resource::HasFlags(ResourceFlags flag) const
    {
        return enumHasAny(m_Flags, flag);
    }

    constexpr bool Resource::IsBuffer() const
    {
        return enumHasAny(m_Flags, ResourceFlags::Buffer);
    }

    constexpr bool Resource::IsImage() const
    {
        return enumHasAny(m_Flags, ResourceFlags::Image);
    }
}

namespace std
{
    template <>
    struct formatter<RG::Resource> {
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }

        auto format(RG::Resource resource, format_context& ctx) const
        {
            return format_to(ctx.out(), "{}", resource.AsString());
        }
    };
}