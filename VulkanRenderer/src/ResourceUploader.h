#pragma once
#include <vector>

#include "Vulkan/Buffer.h"

// todo: use cvars for that, but need to load config first
static constexpr u64 STAGING_BUFFER_DEFAULT_SIZE_BYTES = 1llu * 1024 * 1024;

// used for uploading data by staging buffers
class ResourceUploader
{
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
    
    struct StagingBufferInfo
    {
        Buffer Buffer;
        void* MappedAddress{nullptr};
    };
    
    struct BufferUploadInfo
    {
        u32 SourceIndex;
        const Buffer* Destination;
        BufferCopyInfo CopyInfo;
    };

    struct BufferMappingInfo
    {
        u32 BufferIndex;
        u32 BufferUploadIndex;
    };
public:
    void Init();
    void ShutDown();
    
    void StartRecording();
    void SubmitUpload();
    
    void UpdateBuffer(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset);
    void UpdateBuffer(Buffer& buffer, u32 mappedBufferIndex, u64 bufferOffset);
        u32 GetMappedBuffer(u64 sizeBytes);
    void UpdateBufferImmediately(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset);
    void* GetMappedAddress(u32 mappedBufferIndex);
private:
    StagingBufferInfo CreateStagingBuffer(u64 sizeBytes);
    u64 EnsureCapacity(u64 sizeBytes);
    bool MergeIsPossible(Buffer& buffer, u64 bufferOffset) const;
    bool ShouldBeUpdatedDirectly(Buffer& buffer);
private:
    // ever-growing array of used stage buffers
    std::vector<StagingBufferInfo> m_StageBuffers;
    // index of the last used stage buffer on this frame
    u32 m_LastUsedBuffer{INVALID_INDEX};
    // info about every copy on this frame
    std::vector<BufferUploadInfo> m_BufferUploads;

    // arrays of all mappings done on this frame
    std::vector<BufferMappingInfo> m_ActiveMappings;

    Buffer m_ImmediateUploadBuffer;
};
