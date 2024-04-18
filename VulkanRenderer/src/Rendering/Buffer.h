#pragma once

#include "types.h"

#include "DriverResourceHandle.h"
#include "Core/core.h"

class DeletionQueue;
class CommandBuffer;

class Buffer;

enum class BufferUsage
{
    None = 0,
    Vertex = BIT(1),
    Index = BIT(2),
    Uniform = BIT(3),
    Storage = BIT(4),
    Indirect = BIT(5),
    Upload = BIT(6),
    UploadRandomAccess = BIT(7),
    Readback = BIT(8),
    Source = BIT(9),
    Destination = BIT(10),
    Conditional = BIT(11),
    DeviceAddress = BIT(12),
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
    u64 SizeBytes;
    u64 Offset;
};

struct BufferSubresource
{
    const Buffer* Buffer;
    BufferSubresourceDescription Description;
};
using BufferBindingInfo = BufferSubresource;

class Buffer
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Buffer;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            BufferDescription Description{};
            bool CreateMapped{false};
        };
    public:
        Builder() = default;
        Builder(const BufferDescription& description);
        Buffer Build();
        Buffer Build(DeletionQueue& deletionQueue);
        Buffer BuildManualLifetime();
        Builder& CreateMapped();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Buffer Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Buffer& buffer);

    const BufferDescription& Description() const { return m_Description; }
    
    void SetData(const void* data, u64 dataSizeBytes);
    void SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void* Map();
    void Unmap();
    
    u64 GetSizeBytes() const { return m_Description.SizeBytes; }
    BufferUsage GetKind() const { return m_Description.Usage; }

    BufferSubresource Subresource() const;
    BufferSubresource Subresource(u64 sizeBytes, u64 offset) const;
    BufferSubresource Subresource(const BufferSubresourceDescription& description) const;
    
    BufferBindingInfo BindingInfo() const
    {
        return Subresource();
    }
    BufferBindingInfo BindingInfo(u64 sizeBytes, u64 offset) const
    {
        return Subresource(sizeBytes, offset);
    }

    const void* GetHostAddress() const { return m_HostAddress; }
    void* GetHostAddress() { return m_HostAddress; }

    bool operator==(const Buffer& other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(const Buffer& other) const { return !(*this == other); }
private:
    ResourceHandle<Buffer> Handle() const { return m_ResourceHandle; }
private:
    BufferDescription m_Description{};
    // todo: also store device address?
    void* m_HostAddress{nullptr};
    
    ResourceHandle<Buffer> m_ResourceHandle{};
};