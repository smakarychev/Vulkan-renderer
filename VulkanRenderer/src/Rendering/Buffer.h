#pragma once

#include "types.h"

#include "ResourceHandle.h"
#include "Common/Span.h"
#include "Core/core.h"

class DeletionQueue;
class CommandBuffer;

class Buffer;

enum class BufferUsage
{
    None                    = 0,
    Vertex                  = BIT(1),
    Index                   = BIT(2),
    Uniform                 = BIT(3),
    Storage                 = BIT(4),
    Indirect                = BIT(5),
    Mappable                = BIT(6),
    MappableRandomAccess    = BIT(7),
    Source                  = BIT(8),
    Destination             = BIT(9),
    Conditional             = BIT(10),
    DeviceAddress           = BIT(11),

    Staging                 = Source | Mappable,
    StagingRandomAccess     = Source | MappableRandomAccess,
    Readback                = MappableRandomAccess,

    /* most buffers need this, reads like 'ordinary vertex', 'ordinary storage', etc. */
    Ordinary                = Destination | DeviceAddress,
};

CREATE_ENUM_FLAGS_OPERATORS(BufferUsage)

namespace BufferUtils
{
    std::string bufferUsageToString(BufferUsage usage);
}

struct BufferDescription
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
};

struct BufferSubresourceDescription
{
    u64 SizeBytes{0};
    u64 Offset{0};
};

struct BufferSubresource
{
    const Buffer* Buffer{nullptr};
    BufferSubresourceDescription Description{};
};
using BufferBindingInfo = BufferSubresource;

struct BufferCreateInfo
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
    bool PersistentMapping{false};
    Span<const std::byte> InitialData{};
};

class Buffer
{
    FRIEND_INTERNAL
public:
    const BufferDescription& Description() const { return m_Description; }
    
    void SetData(Span<const std::byte> data);
    void SetData(Span<const std::byte>, u64 offsetBytes);
    void SetData(void* mapped, Span<const std::byte>, u64 offsetBytes);
    void* Map();
    void Unmap();
    
    u64 GetSizeBytes() const { return m_Description.SizeBytes; }
    BufferUsage GetKind() const { return m_Description.Usage; }

    BufferBindingInfo BindingInfo() const
    {
        return BufferSubresource{
            .Buffer = this,
            .Description = {
                .SizeBytes = m_Description.SizeBytes}};
    }
    BufferBindingInfo BindingInfo(u64 sizeBytes, u64 offset) const
    {
        return BufferSubresource{
            .Buffer = this,
            .Description = {
                .SizeBytes = sizeBytes,
                .Offset = offset}};
    }

    const void* GetHostAddress() const { return m_HostAddress; }
    void* GetHostAddress() { return m_HostAddress; }

    bool operator==(const Buffer& other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(const Buffer& other) const { return !(*this == other); }
    ResourceHandleType<Buffer> Handle() const { return m_ResourceHandle; }
private:
    BufferDescription m_Description{};
    // todo: also store device address?
    void* m_HostAddress{nullptr};
    
    ResourceHandleType<Buffer> m_ResourceHandle{};
};