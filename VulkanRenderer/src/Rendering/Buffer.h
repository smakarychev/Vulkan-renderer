﻿#pragma once

#include "types.h"

#include "DriverResourceHandle.h"

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
};

CREATE_ENUM_FLAGS_OPERATORS(BufferUsage)

struct BufferDescription
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
};

struct BufferSubresource
{
    const Buffer* Buffer;
    u64 SizeBytes;
    u64 Offset;
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
        };
    public:
        Builder() = default;
        Builder(const BufferDescription& description);
        Buffer Build();
        Buffer Build(DeletionQueue& deletionQueue);
        Buffer BuildManualLifetime();
        Builder& SetUsage(BufferUsage usage);
        Builder& SetSizeBytes(u64 sizeBytes);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Buffer Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Buffer& buffer);

    void SetData(const void* data, u64 dataSizeBytes);
    void SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void* Map() const;
    void Unmap() const;
    
    u64 GetSizeBytes() const { return m_Description.SizeBytes; }
    BufferUsage GetKind() const { return m_Description.Usage; }

    BufferSubresource CreateSubresource(u64 sizeBytes, u64 offset) const;

    bool operator==(const Buffer& other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(const Buffer& other) const { return !(*this == other); }
private:
    ResourceHandle<Buffer> Handle() const { return m_ResourceHandle; }
private:
    BufferDescription m_Description{};
    ResourceHandle<Buffer> m_ResourceHandle;
};