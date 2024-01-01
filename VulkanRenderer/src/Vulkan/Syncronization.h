#pragma once

#include "VulkanCommon.h"

#include <vulkan/vulkan_core.h>

class CommandBuffer;
class Device;

class Fence
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Fence;
        struct CreateInfo
        {
            VkFenceCreateFlags Flags;
        };
    public:
        Builder& StartSignaled(bool isSignaled = true);
        Fence Build();
        Fence BuildManualLifetime();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Fence Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Fence& semaphore);

    void Reset() const;
    void Wait() const;
    bool Check() const;
private:
    VkFence m_Fence{VK_NULL_HANDLE};
};

class Semaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Semaphore;
        struct CreateInfo{};
    public:
        Semaphore Build();
        Semaphore BuildManualLifetime();
    };
public:
    static Semaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Semaphore& semaphore);
private:
    VkSemaphore m_Semaphore{VK_NULL_HANDLE};
};

class TimelineSemaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class TimelineSemaphore;
        struct CreateInfo
        {
            u64 InitialValue{0};
        };
    public:
        TimelineSemaphore Build();
        TimelineSemaphore BuildManualLifetime();
        Builder& InitialValue(u64 value);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static TimelineSemaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const TimelineSemaphore& semaphore);
    
    void WaitCPU(u64 value);
    void SignalCPU(u64 value);

    u64 GetTimeline() const { return m_Timeline; }
    void SetTimeline(u64 timeline) { m_Timeline = timeline; }
    
private:
    VkSemaphore m_Semaphore{VK_NULL_HANDLE};
    u64 m_Timeline{0};
};