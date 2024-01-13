#pragma once
#include "VulkanCommon.h"

class TimelineSemaphore;
class Semaphore;
class Fence;
struct SwapchainFrameSync;
class CommandPool;
class Device;

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
    std::vector<VkPipelineStageFlags> WaitStages;
    std::vector<Semaphore*> WaitSemaphores;
    std::vector<Semaphore*> SignalSemaphores;
    Fence* Fence;
};

struct BufferSubmitTimelineSyncInfo
{
    std::vector<VkPipelineStageFlags> WaitStages;
    std::vector<TimelineSemaphore*> WaitSemaphores;
    std::vector<u64> WaitValues;
    std::vector<TimelineSemaphore*> SignalSemaphores;
    std::vector<u64> SignalValues;
    Fence* Fence;
};

struct BufferSubmitMixedSyncInfo
{
    std::vector<VkPipelineStageFlags> WaitStages;
    std::vector<Semaphore*> WaitSemaphores;
    std::vector<TimelineSemaphore*> WaitTimelineSemaphores;
    std::vector<u64> WaitValues;
    std::vector<Semaphore*> SignalSemaphores;
    std::vector<TimelineSemaphore*> SignalTimelineSemaphores;
    std::vector<u64> SignalValues;
    Fence* Fence;
};

class CommandBuffer
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class CommandBuffer;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkCommandPool CommandPool;
            VkCommandBufferLevel Level;
            CommandBufferKind Kind;
        };
    public:
        CommandBuffer Build();
        Builder& SetPool(const CommandPool& pool);
        Builder& SetKind(CommandBufferKind kind);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static CommandBuffer Create(const Builder::CreateInfo& createInfo);

    void Reset() const;
    void Begin() const;
    void Begin(CommandBufferUsage commandBufferUsage) const;
    void End() const;

    void Submit(const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync) const;
    void Submit(const QueueInfo& queueInfo, const BufferSubmitTimelineSyncInfo& submitSync) const;
    void Submit(const QueueInfo& queueInfo, const BufferSubmitMixedSyncInfo& submitSync) const;
    void Submit(const QueueInfo& queueInfo, const Fence& fence) const;
    void Submit(const QueueInfo& queueInfo, const Fence* fence) const;
    
private:
    VkCommandBuffer m_CommandBuffer{VK_NULL_HANDLE};
    CommandBufferKind m_Kind{};
};


class CommandPool
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class CommandPool;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            u32 QueueFamily;
            VkCommandPoolCreateFlags Flags;
        };
    public:
        CommandPool Build();
        CommandPool BuildManualLifetime();
        Builder& SetQueue(QueueKind queueKind);
        Builder& PerBufferReset(bool enabled);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static CommandPool Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const CommandPool& commandPool);

    CommandBuffer AllocateBuffer(CommandBufferKind kind);
    void Reset() const;
private:
    VkCommandPool m_CommandPool{VK_NULL_HANDLE};
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