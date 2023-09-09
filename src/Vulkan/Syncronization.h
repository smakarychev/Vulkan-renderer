#pragma once

#include "VulkanCommon.h"

#include <vulkan/vulkan_core.h>

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
        Builder& StartSignaled(bool isSignaled);
        Fence Build();
        Fence BuildManualLifetime();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Fence Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Fence& semaphore);
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