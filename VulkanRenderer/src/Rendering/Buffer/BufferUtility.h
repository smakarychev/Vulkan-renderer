#pragma once

#include "Buffer.h"
#include "PushBuffer.h"
#include "ResourceUploader.h"

struct BufferGrowthPolicy
{
};

struct PushBufferMinimalGrowthPolicy : BufferGrowthPolicy
{
    static u64 GrownSize(u64, u64 necessarySize)
    {
        return necessarySize;
    }
};

struct BufferAsymptoticGrowthPolicy : BufferGrowthPolicy
{
    static u64 GrownSize(u64 currentSize, u64 requiredSize)
    {
        const u64 asymptoticSize = currentSize + (currentSize >> 1) + 1;
        return std::max(asymptoticSize, requiredSize);
    }
};

template <typename Policy>
concept BufferGrowthPolicyConcept = requires(Policy, u64 oldSize, u64 requiredSize)
{
    { std::derived_from<BufferGrowthPolicy, Policy> };
    { Policy::GrownSize(oldSize, requiredSize) } -> std::same_as<u64>;
};

namespace buffers
{
template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
void grow(Buffer buffer, u64 requiredSize, CommandBuffer cmd)
{
    const u64 currentSize = buffer.GetSizeBytes();
    if (currentSize >= requiredSize)
        return;

    const u64 newSize = PushBufferGrowthPolicy::GrownSize(currentSize, requiredSize);
    buffer.Resize(newSize, cmd);
}
}

namespace pushBuffers
{
template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy, typename T>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
void push(PushBuffer& pushBuffer, T&& data, CommandBuffer cmd, ResourceUploader& uploader)
{
    auto&& [_, pushSize] = UploadUtils::getAddressAndSize(data);
    if (pushSize == 0)
        return;
    grow<PushBufferGrowthPolicy>(pushBuffer, pushSize, cmd);
    uploader.UpdateBuffer(pushBuffer.Buffer, std::forward<T>(data), pushBuffer.Offset);
    pushBuffer.Offset += pushSize;
}

template <
    typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy,
    typename T,
    template <typename> typename Range>
    requires
    BufferGrowthPolicyConcept<PushBufferGrowthPolicy> &&
    (is_array_v<Range<T>> || is_vector_v<Range<T>> || is_span_v<Range<T>>)
void push(PushBufferTyped<T>& pushBuffer, Range<T>&& data, CommandBuffer cmd,
    ResourceUploader& uploader)
{
    if (data.size() == 0)
        return;
    grow<PushBufferGrowthPolicy>(pushBuffer, (u32)data.size(), cmd);
    uploader.UpdateBuffer(pushBuffer.Buffer, std::forward<Range<T>>(data), pushBuffer.Offset * sizeof(T));
}

template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
void grow(PushBuffer buffer, u64 pushSize, CommandBuffer cmd)
{
    ::buffers::grow(buffer.Buffer, buffer.Offset + pushSize, cmd);
}

template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy, typename T>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
void grow(PushBufferTyped<T> buffer, u32 pushElements, CommandBuffer cmd)
{
    ::buffers::grow(buffer.Buffer, sizeof(T) * (buffer.Offset + pushElements), cmd);
}
}
