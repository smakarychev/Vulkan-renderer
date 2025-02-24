#pragma once

#include "Core/Traits.h"
#include "Rendering/Buffer/Buffer.h"
#include "Vulkan/Device.h"

#include <vector>

struct FrameContext;
static constexpr u64 STAGING_BUFFER_DEFAULT_SIZE_BYTES = 16llu * 1024 * 1024;
static constexpr u32 STAGING_BUFFER_MAX_IDLE_LIFE_TIME_FRAMES = 300;

namespace UploadUtils
{
    template <typename T>
    std::pair<const void*, u64> getAddressAndSize(T&& data)
    {
        const void* address;
        u64 sizeBytes;
        if constexpr(is_vector_v<T> || is_span_v<T> || is_array_v<T>)
        {
            sizeBytes = data.size() * sizeof(typename std::decay_t<T>::value_type);
            address = data.data();
        }
        else
        {
            sizeBytes = sizeof(T);
            address = &data;
        }

        return {address, sizeBytes};
    }
}

/* used for uploading data by staging buffers */
class ResourceUploader
{
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

    struct StagingBufferInfo
    {
        Buffer Buffer;
        u32 LifeTime{0};
    };
    struct BufferUploadInfo
    {
        Buffer Source{};
        Buffer Destination{};
        u64 SizeBytes{};
        u64 SourceOffset{};
        u64 DestinationOffset{};
    };
public:
    void Init();
    void Shutdown();

    void BeginFrame(const FrameContext& ctx);
    void SubmitUpload(FrameContext& ctx);

    template <typename T>
    void UpdateBuffer(Buffer buffer, T&& data, u64 bufferOffset = 0);

    void CopyBuffer(CopyBufferCommand&& command);

    template <typename T>
    T* MapBuffer(Buffer buffer, u64 bufferOffset = 0);
private:
    void ManageLifeTime();
    StagingBufferInfo CreateStagingBuffer(u64 sizeBytes);
    void EnsureCapacity(u64 sizeBytes);
    bool MergeIsPossible(Buffer buffer, u64 bufferOffset) const;
private:
    struct State
    {
        std::vector<StagingBufferInfo> StageBuffers;
        /* index of the last used stage buffer on this frame */
        u32 LastUsedBuffer{INVALID_INDEX};
        /* info about every copy on this frame */
        std::vector<BufferUploadInfo> BufferUploads;
        /* because of multiple in-frame submits, we have to keep track of already submitted data */
        u32 UploadsOffset{0};

        u64 CurrentBufferOffset{0};
        
        Buffer ImmediateUploadBuffer;
    };
    std::array<State, BUFFERED_FRAMES> m_PerFrameState;
    u32 m_CurrentFrame{0};
};

template <typename T>
void ResourceUploader::UpdateBuffer(Buffer buffer, T&& data, u64 bufferOffset)
{
    if constexpr(std::is_pointer_v<T>)
        LOG("Warning: passing a pointer to `UpdateBuffer`");
    
    auto&& [address, sizeBytes] = UploadUtils::getAddressAndSize(std::forward<T>(data));
        
    EnsureCapacity(sizeBytes);
    auto& state = m_PerFrameState[m_CurrentFrame];
    auto& staging = state.StageBuffers[state.LastUsedBuffer].Buffer;
    Device::SetBufferData(Device::GetBufferMappedAddress(staging),
        Span{(const std::byte*)address, sizeBytes}, state.CurrentBufferOffset);

    if (MergeIsPossible(buffer, bufferOffset))
        state.BufferUploads.back().SizeBytes += sizeBytes;
    else
        state.BufferUploads.push_back({
            .Source = state.StageBuffers[state.LastUsedBuffer].Buffer,
            .Destination = buffer,
            .SizeBytes = sizeBytes,
            .SourceOffset = state.CurrentBufferOffset,
            .DestinationOffset = bufferOffset});
    state.CurrentBufferOffset += sizeBytes;
}

template <typename T>
T* ResourceUploader::MapBuffer(Buffer buffer, u64 bufferOffset)
{
    const usize bufferSize = Device::GetBufferSizeBytes(buffer);
    EnsureCapacity(bufferSize);
    auto& state = m_PerFrameState[m_CurrentFrame];
    const u64 offset = state.CurrentBufferOffset;
    state.BufferUploads.push_back({
        .Source = state.StageBuffers[state.LastUsedBuffer].Buffer,
        .Destination = buffer,
        .SizeBytes = bufferSize,
        .SourceOffset = offset,
        .DestinationOffset = bufferOffset});
    state.CurrentBufferOffset += bufferSize;
    
    return (T*)
        ((std::byte*)Device::GetBufferMappedAddress(state.StageBuffers[state.LastUsedBuffer].Buffer) + offset);      
}

