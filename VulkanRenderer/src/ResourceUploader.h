#pragma once

#include "Core/Traits.h"
#include "Rendering/Buffer.h"
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
        u32 SourceIndex;
        Buffer Destination;
        BufferCopyInfo CopyInfo;
    };
    struct BufferMappingInfo
    {
        u32 BufferIndex;
        u32 BufferUploadIndex;
    };
public:
    void Init();
    void Shutdown();

    void BeginFrame(const FrameContext& ctx);
    void SubmitUpload(CommandBuffer cmd);

    template <typename T>
    void UpdateBuffer(Buffer buffer, T&& data, u64 bufferOffset = 0);
    template <typename T>
    void UpdateBufferImmediately(Buffer buffer, T&& data, u64 bufferOffset = 0);

    template <typename T>
    T* MapBuffer(Buffer buffer, u64 bufferOffset = 0);
private:
    void SubmitImmediateBuffer(Buffer buffer, u64 sizeBytes, u64 offset);
    
    void ManageLifeTime();
    StagingBufferInfo CreateStagingBuffer(u64 sizeBytes);
    u64 EnsureCapacity(u64 sizeBytes);
    bool MergeIsPossible(Buffer buffer, u64 bufferOffset) const;
private:
    struct State
    {
        /* array of used stage buffers */
        std::vector<StagingBufferInfo> StageBuffers;
        /* index of the last used stage buffer on this frame */
        u32 LastUsedBuffer{INVALID_INDEX};
        /* info about every copy on this frame */
        std::vector<BufferUploadInfo> BufferUploads;
        /* because of multiple in-frame submits, we have to keep track of already submitted data */
        u32 UploadsOffset{0};
        
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
        
    u64 stagingOffset = EnsureCapacity(sizeBytes);
    auto& state = m_PerFrameState[m_CurrentFrame];
    auto& staging = state.StageBuffers[state.LastUsedBuffer].Buffer;
    Device::SetBufferData(Device::GetBufferMappedAddress(staging),
        Span{(const std::byte*)address, sizeBytes}, stagingOffset);

    if (MergeIsPossible(buffer, bufferOffset))
        state.BufferUploads.back().CopyInfo.SizeBytes += sizeBytes;
    else
        state.BufferUploads.push_back({
            .SourceIndex = state.LastUsedBuffer,
            .Destination = buffer,
            .CopyInfo = {.SizeBytes = sizeBytes, .SourceOffset = stagingOffset, .DestinationOffset = bufferOffset}});
}

template <typename T>
void ResourceUploader::UpdateBufferImmediately(Buffer buffer, T&& data, u64 bufferOffset)
{
    if constexpr(std::is_pointer_v<T>)
        LOG("Warning: passing a pointer to `UpdateBuffer`");
    
    auto&& [address, sizeBytes] = UploadUtils::getAddressAndSize(std::forward<T>(data));

    auto& state = m_PerFrameState[m_CurrentFrame];
    if (sizeBytes > Device::GetBufferSizeBytes(state.ImmediateUploadBuffer))
    {
        Device::Destroy(state.ImmediateUploadBuffer);
        state.ImmediateUploadBuffer = CreateStagingBuffer(sizeBytes).Buffer;
    }

    Device::SetBufferData(state.ImmediateUploadBuffer, Span{data, sizeBytes});

    SubmitImmediateBuffer(buffer, sizeBytes, bufferOffset);
}

template <typename T>
T* ResourceUploader::MapBuffer(Buffer buffer, u64 bufferOffset)
{
    const usize bufferSize = Device::GetBufferSizeBytes(buffer);
    u64 stagingOffset = EnsureCapacity(bufferSize);
    auto& state = m_PerFrameState[m_CurrentFrame];
    state.BufferUploads.push_back({
        .SourceIndex = state.LastUsedBuffer,
        .Destination = buffer,
        .CopyInfo = {
            .SizeBytes = bufferSize, .SourceOffset = stagingOffset, .DestinationOffset = bufferOffset}});

    return (T*)
        ((std::byte*)Device::GetBufferMappedAddress(state.StageBuffers[state.LastUsedBuffer].Buffer) + stagingOffset);      
}

