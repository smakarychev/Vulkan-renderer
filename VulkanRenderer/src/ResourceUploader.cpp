﻿#include "ResourceUploader.h"

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

void ResourceUploader::Init()
{
    m_StageBuffers.reserve(1);
    m_StageBuffers.push_back(CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES));

    m_ImmediateUploadBuffer = CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES).Buffer;
}

void ResourceUploader::ShutDown()
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
}

void ResourceUploader::SubmitUpload()
{
    if (m_LastUsedBuffer == INVALID_INDEX)
        return;
    
    Driver::ImmediateUpload([this](const CommandBuffer& cmd)
    {
        for (auto& upload : m_BufferUploads)
            RenderCommand::CopyBuffer(cmd, m_StageBuffers[upload.SourceIndex].Buffer, *upload.Destination, upload.CopyInfo);
    });
    
    for (u32 i = 0; i <= m_LastUsedBuffer; i++)
            m_StageBuffers[i].Buffer.Unmap();
}

void ResourceUploader::UpdateBuffer(Buffer& buffer, const void* data, u64 sizeBytes, u64 bufferOffset)
{
    if (ShouldBeUpdatedDirectly(buffer))
    {
        buffer.SetData(data, sizeBytes, bufferOffset);
        return;
    }
    
    u64 stagingOffset = EnsureCapacity(sizeBytes);
    m_StageBuffers[m_LastUsedBuffer].Buffer.SetData(m_StageBuffers[m_LastUsedBuffer].MappedAddress, data, sizeBytes, stagingOffset);
    if (MergeIsPossible(buffer, bufferOffset))
        m_BufferUploads.back().CopyInfo.SizeBytes += sizeBytes;
    else
        m_BufferUploads.push_back({
            .SourceIndex = m_LastUsedBuffer,
            .Destination = &buffer,
            .CopyInfo = {.SizeBytes = sizeBytes, .SourceOffset = stagingOffset, .DestinationOffset = bufferOffset}});
}

void ResourceUploader::UpdateBuffer(Buffer& buffer, u32 mappedBufferIndex, u64 bufferOffset)
{
    m_BufferUploads[m_ActiveMappings[mappedBufferIndex].BufferUploadIndex].Destination = &buffer;
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
    return m_StageBuffers[m_ActiveMappings[mappedBufferIndex].BufferIndex].MappedAddress;
}

ResourceUploader::StagingBufferInfo ResourceUploader::CreateStagingBuffer(u64 sizeBytes)
{
    Buffer stagingBuffer = Buffer::Builder()
        .SetKind(BufferKind::Source)
        .SetSizeBytes(sizeBytes)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
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
        {
            m_StageBuffers.push_back(CreateStagingBuffer(std::max(sizeBytes, STAGING_BUFFER_DEFAULT_SIZE_BYTES)));
            m_StageBuffers[m_LastUsedBuffer].MappedAddress = m_StageBuffers[m_LastUsedBuffer].Buffer.Map();
        }
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
           upload.Destination == &buffer &&
           upload.CopyInfo.DestinationOffset + upload.CopyInfo.SizeBytes == bufferOffset;
}

bool ResourceUploader::ShouldBeUpdatedDirectly(Buffer& buffer)
{
    return (buffer.GetKind().Kind & BufferKind::Destination) == 0;
}