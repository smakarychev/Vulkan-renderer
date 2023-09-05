#pragma once
#include "VulkanCommon.h"

struct SwapchainFrameSync;
class CommandPool;
class Device;

enum class CommandBufferKind {Primary, Secondary};

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
            VkDevice Device;
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

    void Begin();
    void End();

    void Submit(const QueueInfo& queueInfo, const SwapchainFrameSync& frameSync);
    
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
            VkDevice Device;
            u32 QueueFamily;
            VkCommandPoolCreateFlags Flags;
        };
    public:
        CommandPool Build();
        Builder& SetQueue(const Device& device, QueueKind queueKind);
        Builder& PerBufferReset(bool enabled);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static CommandPool Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const CommandPool& commandPool);

    CommandBuffer AllocateBuffer(CommandBufferKind kind);
    
private:
    VkDevice m_Device{VK_NULL_HANDLE};
    VkCommandPool m_CommandPool{VK_NULL_HANDLE};
};