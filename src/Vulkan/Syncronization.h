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
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkDevice Device;
            VkFenceCreateFlags Flags;
        };
    public:
        Builder& SetDevice(const Device& device);
        Builder& StartSignaled(bool isSignaled);
        Fence Build();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Fence Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Fence& semaphore);
private:
    VkFence m_Fence{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
};

class Semaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Semaphore;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkDevice Device;
        };
    public:
        Builder& SetDevice(const Device& device);
        Semaphore Build();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Semaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Semaphore& semaphore);
private:
    VkSemaphore m_Semaphore{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
};