#pragma once
#include <vector>

#include "Rendering/Buffer.h"

// todo: use cvars for that, but need to load config first
static constexpr u64 STAGING_BUFFER_DEFAULT_SIZE_BYTES = 16llu * 1024 * 1024;
static constexpr u32 STAGING_BUFFER_MAX_IDLE_LIFE_TIME_FRAMES = 300;

// used for uploading data by staging buffers
class ResourceUploader
{
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
    
    struct StagingBufferInfo
    {
        Buffer Buffer;
        void* MappedAddress{nullptr};
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

    struct BufferDirectUploadInfo
    {
        Buffer Destination;
        BufferCopyInfo CopyInfo;
    };
public:
    void Init();
    void Shutdown();
    
    void StartRecording();
    void SubmitUpload();

    template <typename T>
    void UpdateBuffer(Buffer& buffer, const T& data);
    template <typename T>
    void UpdateBuffer(Buffer& buffer, const T& data, u64 bufferOffset);
    void UpdateBuffer(Buffer& buffer, const void* data);
    void UpdateBuffer(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset);
    void UpdateBuffer(Buffer& buffer, u32 mappedBufferIndex, u64 bufferOffset);
        u32 GetMappedBuffer(u64 sizeBytes);
    template <typename T>
    void UpdateBufferImmediately(Buffer& buffer, const T& data);
    template <typename T>
    void UpdateBufferImmediately(Buffer& buffer, const T& data, u64 bufferOffset);
    void UpdateBufferImmediately(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset);
    void* GetMappedAddress(u32 mappedBufferIndex);
private:
    void ManageLifeTime();
    StagingBufferInfo CreateStagingBuffer(u64 sizeBytes);
    u64 EnsureCapacity(u64 sizeBytes);
    bool MergeIsPossible(Buffer& buffer, u64 bufferOffset) const;
    bool ShouldBeUpdatedDirectly(Buffer& buffer);
private:
    // array of used stage buffers
    std::vector<StagingBufferInfo> m_StageBuffers;
    // index of the last used stage buffer on this frame
    u32 m_LastUsedBuffer{INVALID_INDEX};
    // info about every copy on this frame
    std::vector<BufferUploadInfo> m_BufferUploads;

    // arrays of all mappings done on this frame
    std::vector<BufferMappingInfo> m_ActiveMappings;

    // info about every update on buffers that can be updated directly
    std::vector<BufferDirectUploadInfo> m_BufferDirectUploads;
    std::vector<u8> m_BufferDirectUploadData;
    
    Buffer m_ImmediateUploadBuffer;
};

template <typename T>
void ResourceUploader::UpdateBuffer(Buffer& buffer, const T& data)
{
    UpdateBuffer(buffer, data, 0);
}

template <typename T>
void ResourceUploader::UpdateBuffer(Buffer& buffer, const T& data, u64 bufferOffset)
{
    UpdateBuffer(buffer, (void*)&data, sizeof(T), bufferOffset);
}

template <typename T>
void ResourceUploader::UpdateBufferImmediately(Buffer& buffer, const T& data)
{
    UpdateBufferImmediately(buffer, data);
}

template <typename T>
void ResourceUploader::UpdateBufferImmediately(Buffer& buffer, const T& data, u64 bufferOffset)
{
    UpdateBufferImmediately(buffer, (void*)&data, sizeof(T), bufferOffset);
}
