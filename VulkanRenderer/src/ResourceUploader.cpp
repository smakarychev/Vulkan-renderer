#include "ResourceUploader.h"

#include <algorithm>
#include <tracy/Tracy.hpp>

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

void ResourceUploader::Init()
{
    m_StageBuffers.reserve(1);
    m_StageBuffers.push_back(CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES));

    m_ImmediateUploadBuffer = CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES).Buffer;
}

void ResourceUploader::Shutdown()
{
    for (auto& buffer : m_StageBuffers)
        Buffer::Destroy(buffer.Buffer);
    Buffer::Destroy(m_ImmediateUploadBuffer);
}

void ResourceUploader::StartRecording()
{
    m_LastUsedBuffer = INVALID_INDEX;
    m_BufferUploads.clear();
    m_ActiveMappings.clear();
    m_BufferDirectUploadData.clear();
    m_BufferDirectUploads.clear();
}

void ResourceUploader::SubmitUpload()
{
    ZoneScopedN("Submit Upload");

    for (auto& directUpload : m_BufferDirectUploads)
        directUpload.Destination.SetData(
            m_BufferDirectUploadData.data() + directUpload.CopyInfo.SourceOffset,
            directUpload.CopyInfo.SizeBytes,
            directUpload.CopyInfo.DestinationOffset);

    ManageLifeTime();
    
    if (m_LastUsedBuffer == INVALID_INDEX)
        return;
    
    Driver::ImmediateUpload([this](const CommandBuffer& cmd)
    {
        for (auto& upload : m_BufferUploads)
            RenderCommand::CopyBuffer(cmd, m_StageBuffers[upload.SourceIndex].Buffer, upload.Destination, upload.CopyInfo);
    });
    
    for (u32 i = 0; i <= m_LastUsedBuffer; i++)
        m_StageBuffers[i].Buffer.Unmap();
}

void ResourceUploader::UpdateBuffer(Buffer& buffer, const void* data)
{
    UpdateBuffer(buffer, data, buffer.GetSizeBytes(), 0);
}

void ResourceUploader::UpdateBuffer(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset)
{
    if (ShouldBeUpdatedDirectly(buffer))
    {
        u64 sourceOffset = m_BufferDirectUploadData.size();
        m_BufferDirectUploadData.resize(sourceOffset + sizeBytes);
        std::memcpy(m_BufferDirectUploadData.data() + sourceOffset, data, sizeBytes);
        m_BufferDirectUploads.push_back({
            .Destination = buffer,
            .CopyInfo = {.SizeBytes = sizeBytes, .SourceOffset = sourceOffset, .DestinationOffset = bufferOffset}});

        return;
    }
    
    u64 stagingOffset = EnsureCapacity(sizeBytes);
    m_StageBuffers[m_LastUsedBuffer].Buffer.SetData(m_StageBuffers[m_LastUsedBuffer].MappedAddress, data, sizeBytes, stagingOffset);
    if (MergeIsPossible(buffer, bufferOffset))
        m_BufferUploads.back().CopyInfo.SizeBytes += sizeBytes;
    else
        m_BufferUploads.push_back({
            .SourceIndex = m_LastUsedBuffer,
            .Destination = buffer,
            .CopyInfo = {.SizeBytes = sizeBytes, .SourceOffset = stagingOffset, .DestinationOffset = bufferOffset}});
}

void ResourceUploader::UpdateBuffer(Buffer& buffer, u32 mappedBufferIndex, u64 bufferOffset)
{
    m_BufferUploads[m_ActiveMappings[mappedBufferIndex].BufferUploadIndex].Destination = buffer;
    m_BufferUploads[m_ActiveMappings[mappedBufferIndex].BufferUploadIndex].CopyInfo.DestinationOffset = bufferOffset;
}

u32 ResourceUploader::GetMappedBuffer(u64 sizeBytes)
{
    u64 stagingOffset = EnsureCapacity(sizeBytes);

    u32 mappingIndex = (u32)m_ActiveMappings.size();
    m_ActiveMappings.push_back({.BufferIndex = m_LastUsedBuffer, .BufferUploadIndex = (u32)m_BufferUploads.size()});
    
    m_BufferUploads.push_back({
        .SourceIndex = m_LastUsedBuffer,
        .CopyInfo = {.SizeBytes = sizeBytes, .SourceOffset = stagingOffset}});

    return mappingIndex;
}

void ResourceUploader::UpdateBufferImmediately(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset)
{
    if (ShouldBeUpdatedDirectly(buffer))
    {
        buffer.SetData(data, sizeBytes, bufferOffset);
        
        return;
    }
    
    if (sizeBytes > m_ImmediateUploadBuffer.GetSizeBytes())
        m_ImmediateUploadBuffer = CreateStagingBuffer(sizeBytes).Buffer;

    m_ImmediateUploadBuffer.SetData(data, sizeBytes);
    
    Driver::ImmediateUpload([&](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBuffer(cmd, m_ImmediateUploadBuffer, buffer,
            {.SizeBytes = sizeBytes, .SourceOffset = 0, .DestinationOffset = bufferOffset});        
    });
}

void* ResourceUploader::GetMappedAddress(u32 mappedBufferIndex)
{
    u64 offset = m_BufferUploads[m_ActiveMappings[mappedBufferIndex].BufferUploadIndex].CopyInfo.SourceOffset;
    u8* address = (u8*)m_StageBuffers[m_ActiveMappings[mappedBufferIndex].BufferIndex].MappedAddress;
    return address + offset;
}

void ResourceUploader::ManageLifeTime()
{
    if (m_LastUsedBuffer == INVALID_INDEX)
    {
        for (auto& stageBuffer : m_StageBuffers)
            stageBuffer.LifeTime++;
    }
    else
    {
        for (u32 i = 0; i <= m_LastUsedBuffer; i++)
            m_StageBuffers[i].LifeTime = 0;
        
        for (u32 i = m_LastUsedBuffer + 1; i < m_StageBuffers.size(); i++)
            m_StageBuffers[i].LifeTime++;
    }

    auto it = std::ranges::remove_if(m_StageBuffers,
        [](const auto& stageBufferInfo) { return stageBufferInfo.LifeTime > MAX_PIPELINE_DESCRIPTOR_SETS; }).begin();

    for (auto toDelete = it; toDelete != m_StageBuffers.end(); toDelete++)
        Buffer::Destroy(toDelete->Buffer);

    m_StageBuffers.erase(it, m_StageBuffers.end());
}

ResourceUploader::StagingBufferInfo ResourceUploader::CreateStagingBuffer(u64 sizeBytes)
{
    Buffer stagingBuffer = Buffer::Builder()
        .SetUsage(BufferUsage::Source | BufferUsage::Upload)
        .SetSizeBytes(sizeBytes)
        .BuildManualLifetime();

    return {.Buffer = stagingBuffer, .MappedAddress = nullptr};
}

u64 ResourceUploader::EnsureCapacity(u64 sizeBytes)
{
    u64 currentBufferOffset = 0;
    // check that we have any buffer at all
    if (m_LastUsedBuffer == INVALID_INDEX)
    {
        m_LastUsedBuffer = 0;
        m_StageBuffers[m_LastUsedBuffer].MappedAddress = m_StageBuffers[m_LastUsedBuffer].Buffer.Map();
    }
    else
    {
        currentBufferOffset = m_BufferUploads.back().CopyInfo.SourceOffset + m_BufferUploads.back().CopyInfo.SizeBytes;
    }

    if (m_StageBuffers[m_LastUsedBuffer].Buffer.GetSizeBytes() < currentBufferOffset + sizeBytes)
    {
        m_LastUsedBuffer++;
        if (m_LastUsedBuffer == m_StageBuffers.size())
            m_StageBuffers.push_back(CreateStagingBuffer(std::max(sizeBytes, STAGING_BUFFER_DEFAULT_SIZE_BYTES)));

        if (m_StageBuffers[m_LastUsedBuffer].Buffer.GetSizeBytes() < sizeBytes)
        {
            Buffer::Destroy(m_StageBuffers[m_LastUsedBuffer].Buffer);
            m_StageBuffers[m_LastUsedBuffer] = CreateStagingBuffer(sizeBytes);
        }
        
        m_StageBuffers[m_LastUsedBuffer].MappedAddress = m_StageBuffers[m_LastUsedBuffer].Buffer.Map();
        currentBufferOffset = 0;
    }

    return currentBufferOffset;
}

bool ResourceUploader::MergeIsPossible(Buffer& buffer, u64 bufferOffset) const
{
    if (m_BufferUploads.empty())
        return false;

    const BufferUploadInfo& upload = m_BufferUploads.back();
    
    return upload.SourceIndex == m_LastUsedBuffer &&
           upload.Destination == buffer &&
           upload.CopyInfo.DestinationOffset + upload.CopyInfo.SizeBytes == bufferOffset;
}

bool ResourceUploader::ShouldBeUpdatedDirectly(Buffer& buffer)
{
    return !enumHasAll(buffer.GetKind(), BufferUsage::Destination);
}
