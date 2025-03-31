#include "ResourceUploader.h"

#include <algorithm>
#include <ranges>
#include <tracy/Tracy.hpp>

#include "FrameContext.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Commands/RenderCommands.h"
#include "Vulkan/Device.h"

void ResourceUploader::Init()
{
    static constexpr u64 USE_DEFAULT_SIZE = 0;
    for (auto& state : m_PerFrameState)
    {
        state.StageBuffers.reserve(1);
        state.StageBuffers.push_back(CreateStagingBuffer(USE_DEFAULT_SIZE));
        state.ImmediateUploadBuffer = CreateStagingBuffer(USE_DEFAULT_SIZE).Buffer;
    }
}

void ResourceUploader::Shutdown()
{
    for (auto& state : m_PerFrameState)
    {
        for (auto& buffer : state.StageBuffers)
            Device::Destroy(buffer.Buffer);
        Device::Destroy(state.ImmediateUploadBuffer);
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

void ResourceUploader::SubmitUpload(FrameContext& ctx)
{
    CPU_PROFILE_FRAME("Submit Upload")

    auto& state = m_PerFrameState[m_CurrentFrame];
    if (state.LastUsedBuffer == INVALID_INDEX)
        return;

    for (u32 i = state.UploadsOffset; i < state.BufferUploads.size(); i++)
    {
        auto& upload = state.BufferUploads[i];

        ctx.CommandList.CopyBuffer({
            .Source = upload.Source,
            .Destination = upload.Destination,
            .SizeBytes = upload.SizeBytes,
            .SourceOffset = upload.SourceOffset,
            .DestinationOffset = upload.DestinationOffset});
    }

    state.UploadsOffset = (u32)state.BufferUploads.size();
}

void ResourceUploader::CopyBuffer(CopyBufferCommand&& command)
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    state.BufferUploads.push_back({
        .Source = command.Source,
        .Destination = command.Destination,
        .SizeBytes = command.SizeBytes,
        .SourceOffset = command.SourceOffset,
        .DestinationOffset = command.DestinationOffset});
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
            static u64 lifeTime = CVars::Get().GetI32CVar("Uploader.StagingLifetime"_hsv,
                STAGING_BUFFER_MAX_IDLE_LIFE_TIME_FRAMES);
            return stageBufferInfo.LifeTime > lifeTime;
        }).begin();

    for (auto toDelete = it; toDelete != state.StageBuffers.end(); toDelete++)
        Device::Destroy(toDelete->Buffer);

    state.StageBuffers.erase(it, state.StageBuffers.end());
}

ResourceUploader::StagingBufferInfo ResourceUploader::CreateStagingBuffer(u64 sizeBytes)
{
    static u64 minSizeBytes = CVars::Get().GetI32CVar("Uploader.StagingSizeBytes"_hsv, STAGING_BUFFER_DEFAULT_SIZE_BYTES);

    return {.Buffer = Device::CreateStagingBuffer(std::max(minSizeBytes, sizeBytes))};
}

void ResourceUploader::EnsureCapacity(u64 sizeBytes)
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    /* check that we have any buffer at all */
    if (state.LastUsedBuffer == INVALID_INDEX)
        state.LastUsedBuffer = 0;

    if (Device::GetBufferSizeBytes(state.StageBuffers[state.LastUsedBuffer].Buffer) <
        state.CurrentBufferOffset + sizeBytes)
    {
        state.LastUsedBuffer++;
        if (state.LastUsedBuffer == state.StageBuffers.size())
            state.StageBuffers.push_back(CreateStagingBuffer(sizeBytes));

        if (Device::GetBufferSizeBytes(state.StageBuffers[state.LastUsedBuffer].Buffer) < sizeBytes)
        {
            Device::Destroy(state.StageBuffers[state.LastUsedBuffer].Buffer);
            state.StageBuffers[state.LastUsedBuffer] = CreateStagingBuffer(sizeBytes);
        }
    
        state.CurrentBufferOffset = 0;
    }
}

bool ResourceUploader::MergeIsPossible(Buffer buffer, u64 bufferOffset) const
{
    auto& state = m_PerFrameState[m_CurrentFrame];

    if (state.BufferUploads.empty())
        return false;

    const BufferUploadInfo& upload = state.BufferUploads.back();

    return upload.Source == state.StageBuffers[state.LastUsedBuffer].Buffer &&
           upload.Destination == buffer &&
           upload.DestinationOffset + upload.SizeBytes == bufferOffset;
}
