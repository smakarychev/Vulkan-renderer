#pragma once
#include "VulkanCommon.h"

class Semaphore;
class Fence;
struct SwapchainFrameSync;
class CommandPool;
class Device;

enum class CommandBufferKind {Primary, Secondary};

struct BufferSubmitSyncInfo
{
    std::vector<Semaphore*> WaitSemaphores;
    std::vector<Semaphore*> SignalSemaphores;
    std::vector<VkPipelineStageFlags> WaitStages;
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
    void End() const;

    void Submit(const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync) const;
    void Submit(const QueueInfo& queueInfo, const Fence& fence) const;
    
private:
    VkCommandBuffer m_CommandBuffer{VK_NULL_HANDLE};
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