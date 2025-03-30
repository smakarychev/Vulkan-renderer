#pragma once

#include "Buffer.h"
#include "PushBuffer.h"
#include "ResourceUploader.h"
#include "Vulkan/Device.h"

struct BufferGrowthPolicy {};
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

namespace Buffers
{
    template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
    void grow(Buffer buffer, u64 requiredSize, RenderCommandList& cmdList)
    {
        const u64 currentSize = Device::GetBufferSizeBytes(buffer);
        if (currentSize >= requiredSize)
            return;

        const u64 newSize = PushBufferGrowthPolicy::GrownSize(currentSize, requiredSize);
        Device::ResizeBuffer(buffer, newSize, cmdList);
    }
}

namespace PushBuffers
{
    template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy, typename T>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
    void push(PushBuffer pushBuffer, T&& data, RenderCommandList& cmdList, ResourceUploader& uploader)
    {
        auto&& [_, pushSize] = UploadUtils::getAddressAndSize(data);
        grow<PushBufferGrowthPolicy>(pushBuffer, pushSize, cmdList);
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
    void push(PushBufferTyped<T> pushBuffer, Range<T>&& data, RenderCommandList& cmdList,
        ResourceUploader& uploader)
    {
        grow<PushBufferGrowthPolicy>(pushBuffer, (u32)data.size(), cmdList);
        uploader.UpdateBuffer(pushBuffer.Buffer, std::forward<Range<T>>(data), pushBuffer.Offset * sizeof(T));
        pushBuffer.Offset += (u32)data.size();
    }

    template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
    void grow(PushBuffer buffer, u64 pushSize, RenderCommandList& cmdList)
    {
        ::Buffers::grow(buffer.Buffer, buffer.Offset + pushSize, cmdList);
    }

    template <typename PushBufferGrowthPolicy = PushBufferMinimalGrowthPolicy, typename T>
    requires BufferGrowthPolicyConcept<PushBufferGrowthPolicy>
    void grow(PushBufferTyped<T> buffer, u32 pushElements, RenderCommandList& cmdList)
    {
        ::Buffers::grow(buffer.Buffer, sizeof(T) * (buffer.Offset + pushElements), cmdList);
    }
}