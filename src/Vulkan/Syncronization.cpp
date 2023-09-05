#include "Syncronization.h"

#include "Driver.h"

Fence Fence::Builder::Build()
{
    Fence fence = Fence::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([fence](){ Fence::Destroy(fence); });

    return fence;
}

Fence::Builder& Fence::Builder::SetDevice(const Device& device)
{
    Driver::Unpack(device, m_CreateInfo);

    return *this;
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

    VulkanCheck(vkCreateFence(createInfo.Device, &fenceCreateInfo, nullptr, &fence.m_Fence),
        "Failed to create fence");
    fence.m_Device = createInfo.Device;

    return fence;
}

void Fence::Destroy(const Fence& semaphore)
{
    vkDestroyFence(semaphore.m_Device, semaphore.m_Fence, nullptr);
}

Semaphore Semaphore::Builder::Build()
{
    Semaphore semaphore = Semaphore::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([semaphore](){ Semaphore::Destroy(semaphore); });

    return semaphore;
}

Semaphore::Builder& Semaphore::Builder::SetDevice(const Device& device)
{
    Driver::Unpack(device, m_CreateInfo);

    return *this;
}


Semaphore Semaphore::Create(const Builder::CreateInfo& createInfo)
{
    Semaphore semaphore = {};

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VulkanCheck(vkCreateSemaphore(createInfo.Device, &semaphoreCreateInfo, nullptr, &semaphore.m_Semaphore),
        "Failed to create semaphore");
    semaphore.m_Device = createInfo.Device;
    
    return semaphore;
}

void Semaphore::Destroy(const Semaphore& semaphore)
{
    vkDestroySemaphore(semaphore.m_Device, semaphore.m_Semaphore, nullptr);
}
