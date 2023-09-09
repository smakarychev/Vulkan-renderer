#include "Syncronization.h"

#include "Driver.h"

Fence Fence::Builder::Build()
{
    Fence fence = Fence::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([fence](){ Fence::Destroy(fence); });

    return fence;
}

Fence::Builder& Fence::Builder::StartSignaled(bool isSignaled)
{
    if (isSignaled)
        m_CreateInfo.Flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        m_CreateInfo.Flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    return *this;
}


Fence Fence::Create(const Builder::CreateInfo& createInfo)
{
    Fence fence = {};
    
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = createInfo.Flags;

    VulkanCheck(vkCreateFence(Driver::DeviceHandle(), &fenceCreateInfo, nullptr, &fence.m_Fence),
        "Failed to create fence");

    return fence;
}

void Fence::Destroy(const Fence& semaphore)
{
    vkDestroyFence(Driver::DeviceHandle(), semaphore.m_Fence, nullptr);
}

Semaphore Semaphore::Builder::Build()
{
    Semaphore semaphore = Semaphore::Create({});
    Driver::DeletionQueue().AddDeleter([semaphore](){ Semaphore::Destroy(semaphore); });

    return semaphore;
}

Semaphore Semaphore::Create(const Builder::CreateInfo& createInfo)
{
    Semaphore semaphore = {};

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VulkanCheck(vkCreateSemaphore(Driver::DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphore.m_Semaphore),
        "Failed to create semaphore");
    Driver::DeviceHandle();
    
    return semaphore;
}

void Semaphore::Destroy(const Semaphore& semaphore)
{
    vkDestroySemaphore(Driver::DeviceHandle(), semaphore.m_Semaphore, nullptr);
}
