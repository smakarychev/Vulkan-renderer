#pragma once

#include "RGResource.h"
#include "Rendering/RenderingInfo.h"
#include "Rendering/SynchronizationTraits.h"

namespace RG
{
    enum class ResourceAccessFlags
    {
        None = 0,

        Vertex      = BIT(1),
        Pixel       = BIT(2),
        Compute     = BIT(3),

        Index       = BIT(4),
        Indirect    = BIT(5),
        Conditional = BIT(6),
        
        Attribute   = BIT(7),
        Uniform     = BIT(8),
        Storage     = BIT(9),

        Sampled     = BIT(10),

        Blit        = BIT(11),
        Copy        = BIT(12),

        Readback    = BIT(13),
    };
    CREATE_ENUM_FLAGS_OPERATORS(ResourceAccessFlags)

    enum class AccessType : u8
    {
        None = 0,
        Read    = BIT(1),
        Write   = BIT(2),
        Split   = BIT(3),
        Merge   = BIT(4),

        ReadWrite = Read | Write,
    };
    CREATE_ENUM_FLAGS_OPERATORS(AccessType)
    
    struct ResourceAccess
    {
        static constexpr u32 NO_PASS = ~0u;
        u32 PassIndex{NO_PASS};
        Resource Resource{};
        AccessType Type{AccessType::None};
        PipelineStage Stage{PipelineStage::None};
        PipelineAccess Access{PipelineAccess::None};

        bool OfType(AccessType type) const { return enumHasAny(Type, type); }
        bool IsSplitOrMerge() const { return enumHasAny(Type, AccessType::Split | AccessType::Merge); }
        bool HasReadOrWrite() const { return enumHasAny(Type, AccessType::ReadWrite); }
        bool HasRead() const { return enumHasAny(Type, AccessType::Read); }
        bool HasWrite() const { return enumHasAny(Type, AccessType::Write); }
        bool IsReadOnly() const { return Type == AccessType::Read; }
    };

    struct RenderTargetAccessDescription
    {
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        AttachmentStore OnStore{AttachmentStore::Store};
        ColorClearValue ClearColor{};
    };

    struct DepthStencilTargetAccessDescription
    {
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        AttachmentStore OnStore{AttachmentStore::Store};
        DepthStencilClearValue ClearDepthStencil{};
    };

    struct RenderTargetAccess
    {
        Resource Resource{};
        RenderTargetAccessDescription Description{};
    };

    struct DepthStencilTargetAccess
    {
        Resource Resource{};
        DepthStencilTargetAccessDescription Description{};
        std::optional<DepthBias> DepthBias{std::nullopt};
    };

    enum class AccessConflictType : u8
    {
        Execution, Memory, Layout
    };
    struct ResourceAccessConflict
    {
        AccessConflictType Type{AccessConflictType::Execution};
        u32 FirstPassIndex{0};
        u32 SecondPassIndex{0};
        Resource Resource{};
        PipelineStage FirstStage{PipelineStage::None};
        PipelineStage SecondStage{PipelineStage::None};
        PipelineAccess FirstAccess{PipelineAccess::None};
        PipelineAccess SecondAccess{PipelineAccess::None};
    };
    using BufferResourceAccessConflict = ResourceAccessConflict;
    struct ImageResourceAccessConflict
    {
        ResourceAccessConflict AccessConflict{};
        ImageLayout FirstLayout{};
        ImageLayout SecondLayout{};
        ImageSubresourceDescription Subresource{};
    };
}
