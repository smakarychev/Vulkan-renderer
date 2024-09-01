#include "ResourceUploader.h"

#include <algorithm>
#include <ranges>
#include <tracy/Tracy.hpp>

#include "FrameContext.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

void ResourceUploader::Init()
{
    for (auto& state : m_PerFrameState)
    {
        state.StageBuffers.reserve(1);
        state.StageBuffers.push_back(CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES));
        state.ImmediateUploadBuffer = CreateStagingBuffer(STAGING_BUFFER_DEFAULT_SIZE_BYTES).Buffer;
    }
}

void ResourceUploader::Shutdown()
{
    for (auto& state : m_PerFrameState)
    {
        for (auto& buffer : state.StageBuffers)
            Buffer::Destroy(buffer.Buffer);
        Buffer::Destroy(state.ImmediateUploadBuffer);
    }
}

void ResourceUploader::BeginFrame(const FrameContext& ctx)
{
    m_CurrentFrame = ctx.FrameNumber;
    ManageLifeTime();

    auto& state = m_PerFrameState[m_CurrentFrame];
    state.LastUsedBuffer = INVALID_INDEX;
    state.BufferUploads.clear();
    state.UploadsOffset = 0;
}

void ResourceUploader::SubmitUpload(const CommandBuffer& cmd)
{
    CPU_PROFILE_FRAME("Submit Upload")

    auto& state = m_PerFrameState[m_CurrentFrame];
    if (state.LastUsedBuffer == INVALID_INDEX)
        return;

    for (u32 i = state.UploadsOffset; i < state.BufferUploads.size(); i++)
    {
        auto& upload = state.BufferUploads[i];
        RenderCommand::CopyBuffer(cmd, state.StageBuffers[upload.SourceIndex].Buffer,
            upload.Destination, upload.CopyInfo);
    }

    state.UploadsOffset = (u32)state.BufferUploads.size();
}

void ResourceUploader::SubmitImmediateBuffer(const Buffer& buffer, u64 sizeBytes, u64 offset)
{
    Driver::ImmediateSubmit([&](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBuffer(cmd, m_PerFrameState[m_CurrentFrame].ImmediateUploadBuffer, buffer,
            {.SizeBytes = sizeBytes, .SourceOffset = 0, .DestinationOffset = offset});        
    });
}

void ResourceUploader::ManageLifeTime()
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    u32 lastUsed = std::min(state.LastUsedBuffer, (u32)state.StageBuffers.size() - 1);
    for (auto& buffer : state.StageBuffers | std::ranges::views::take(lastUsed + 1))
        buffer.LifeTime = 0;
    for (auto& buffer : state.StageBuffers | std::ranges::views::drop(lastUsed + 1))
        buffer.LifeTime++;

    auto it = std::ranges::remove_if(state.StageBuffers,
        [](const auto& stageBufferInfo)
        {
            return stageBufferInfo.LifeTime > STAGING_BUFFER_MAX_IDLE_LIFE_TIME_FRAMES;
        }).begin();

    for (auto toDelete = it; toDelete != state.StageBuffers.end(); toDelete++)
        Buffer::Destroy(toDelete->Buffer);

    state.StageBuffers.erase(it, state.StageBuffers.end());
}

ResourceUploader::StagingBufferInfo ResourceUploader::CreateStagingBuffer(u64 sizeBytes)
{
    Buffer stagingBuffer = Buffer::Builder({
            .SizeBytes = sizeBytes,
            .Usage = BufferUsage::Staging})
        .CreateMapped()
        .BuildManualLifetime();

    return {.Buffer = stagingBuffer};
}

u64 ResourceUploader::EnsureCapacity(u64 sizeBytes)
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    u64 currentBufferOffset = 0;
    /* check that we have any buffer at all */
    if (state.LastUsedBuffer == INVALID_INDEX)
    {
        state.LastUsedBuffer = 0;
    }
    else
    {
        currentBufferOffset = state.BufferUploads.back().CopyInfo.SourceOffset +
            state.BufferUploads.back().CopyInfo.SizeBytes;
    }

    if (state.StageBuffers[state.LastUsedBuffer].Buffer.GetSizeBytes() < currentBufferOffset + sizeBytes)
    {
        state.LastUsedBuffer++;
        if (state.LastUsedBuffer == state.StageBuffers.size())
            state.StageBuffers.push_back(CreateStagingBuffer(std::max(sizeBytes, STAGING_BUFFER_DEFAULT_SIZE_BYTES)));

        if (state.StageBuffers[state.LastUsedBuffer].Buffer.GetSizeBytes() < sizeBytes)
        {
            Buffer::Destroy(state.StageBuffers[state.LastUsedBuffer].Buffer);
            state.StageBuffers[state.LastUsedBuffer] = CreateStagingBuffer(sizeBytes);
        }
    
        currentBufferOffset = 0;
    }

    return currentBufferOffset;
}

bool ResourceUploader::MergeIsPossible(const Buffer& buffer, u64 bufferOffset) const
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    if (state.BufferUploads.empty())
        return false;

    const BufferUploadInfo& upload = state.BufferUploads.back();

    return upload.SourceIndex == state.LastUsedBuffer &&
           upload.Destination == buffer &&
           upload.CopyInfo.DestinationOffset + upload.CopyInfo.SizeBytes == bufferOffset;
}
