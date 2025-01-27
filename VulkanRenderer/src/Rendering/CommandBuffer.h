#pragma once

#include "ResourceHandle.h"
#include "Synchronization.h"
#include "SynchronizationTraits.h"

class DeletionQueue;
class QueueInfo;
struct SwapchainFrameSync;

enum class CommandBufferKind {Primary, Secondary};
enum class CommandBufferUsage
{
    SingleSubmit = BIT(1),
    MultipleSubmit = BIT(2),
    SimultaneousUse = BIT(3),
};
CREATE_ENUM_FLAGS_OPERATORS(CommandBufferUsage)

struct BufferSubmitSyncInfo
{
    Span<const PipelineStage> WaitStages;
    Span<const Semaphore> WaitSemaphores;
    Span<const Semaphore> SignalSemaphores;
    Fence Fence{};
};

struct BufferSubmitTimelineSyncInfo
{
    Span<const PipelineStage> WaitStages;
    Span<const TimelineSemaphore> WaitSemaphores;
    Span<const u64> WaitValues;
    Span<const TimelineSemaphore> SignalSemaphores;
    Span<const u64> SignalValues;
    Fence Fence{};
};

struct CommandPoolCreateInfo
{
    QueueKind QueueKind{QueueKind::Graphics};
    bool PerBufferReset{false};
};

struct CommandPoolTag{};
using CommandPool = ResourceHandleType<CommandPoolTag>;

struct CommandBufferCreateInfo
{
    CommandPool Pool{};
    CommandBufferKind Kind{CommandBufferKind::Primary};
};

class CommandBuffer
{
    FRIEND_INTERNAL
public:
    void Reset() const;
    void Begin() const;
    void Begin(CommandBufferUsage commandBufferUsage) const;
    void End() const;

    void Submit(QueueKind queueKind, const BufferSubmitSyncInfo& submitSync) const;
    void Submit(QueueKind queueKind, const BufferSubmitTimelineSyncInfo& submitSync) const;
    void Submit(QueueKind queueKind, Fence fence) const;
private:
    ResourceHandleType<CommandBuffer> Handle() const { return m_ResourceHandle; }
private:
    CommandBufferKind m_Kind{};
    ResourceHandleType<CommandBuffer> m_ResourceHandle{};
};

class CommandBufferArray
{
public:
    CommandBufferArray(QueueKind queueKind, bool individualReset);
    const CommandBuffer& GetBuffer() const { return m_Buffers[m_CurrentIndex]; }
    CommandBuffer& GetBuffer() { return m_Buffers[m_CurrentIndex]; }
    void SetIndex(u32 index) { m_CurrentIndex = index; EnsureCapacity(m_CurrentIndex); }
    void NextIndex() { m_CurrentIndex++; EnsureCapacity(m_CurrentIndex); }

    void ResetBuffers();

private:
    void EnsureCapacity(u32 index);
private:
    std::vector<CommandBuffer> m_Buffers;
    CommandPool m_Pool;
    u32 m_CurrentIndex{0};
};