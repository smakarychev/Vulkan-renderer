#pragma once

#include "ResourceHandle.h"
#include "SynchronizationTraits.h"

class DeletionQueue;
class QueueInfo;
class TimelineSemaphore;
class Semaphore;
class Fence;
struct SwapchainFrameSync;
class CommandPool;

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
    std::vector<PipelineStage> WaitStages;
    std::vector<Semaphore*> WaitSemaphores;
    std::vector<Semaphore*> SignalSemaphores;
    Fence* Fence;
};

struct BufferSubmitTimelineSyncInfo
{
    std::vector<PipelineStage> WaitStages;
    std::vector<TimelineSemaphore*> WaitSemaphores;
    std::vector<u64> WaitValues;
    std::vector<TimelineSemaphore*> SignalSemaphores;
    std::vector<u64> SignalValues;
    Fence* Fence;
};

struct CommandBufferCreateInfo
{
    // todo: change to handle, once everything is handle
    const CommandPool* Pool{nullptr};
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
    void Submit(QueueKind queueKind, const Fence& fence) const;
    void Submit(QueueKind queueKind, const Fence* fence) const;
private:
    ResourceHandleType<CommandBuffer> Handle() const { return m_ResourceHandle; }
private:
    CommandBufferKind m_Kind{};
    ResourceHandleType<CommandBuffer> m_ResourceHandle{};
};

struct CommandPoolCreateInfo
{
    QueueKind QueueKind{QueueKind::Graphics};
    bool PerBufferReset{false};
};

class CommandPool
{
    FRIEND_INTERNAL
public:
    CommandBuffer AllocateBuffer(CommandBufferKind kind);
    void Reset() const;
private:
    ResourceHandleType<CommandPool> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<CommandPool> m_ResourceHandle{};
};

class CommandBufferArray
{
public:
    CommandBufferArray(QueueKind queueKind, bool individualReset);
    const CommandBuffer& GetBuffer() const { return m_Buffers[m_CurrentIndex]; }
    CommandBuffer& GetBuffer() { return m_Buffers[m_CurrentIndex]; }
    void SetIndex(u32 index) { m_CurrentIndex = index; EnsureCapacity(m_CurrentIndex); }
    void NextIndex() { m_CurrentIndex++; EnsureCapacity(m_CurrentIndex); }

    void ResetBuffers() { m_Pool.Reset(); }
    
private:
    void EnsureCapacity(u32 index);
private:
    std::vector<CommandBuffer> m_Buffers;
    CommandPool m_Pool;
    u32 m_CurrentIndex{0};
};