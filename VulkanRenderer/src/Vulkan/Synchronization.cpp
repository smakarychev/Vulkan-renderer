#include "Synchronization.h"

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanCore.h"

Fence Fence::Builder::Build()
{
    Fence fence = Fence::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([fence](){ Fence::Destroy(fence); });

    return fence;
}

Fence Fence::Builder::BuildManualLifetime()
{
    return Fence::Create(m_CreateInfo);
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

void Fence::Reset() const
{
    RenderCommand::ResetFence(*this);
}

void Fence::Wait() const
{
    RenderCommand::WaitForFence(*this);
}

bool Fence::Check() const
{
    return RenderCommand::CheckFence(*this) == VK_SUCCESS;
}

Semaphore Semaphore::Builder::Build()
{
    Semaphore semaphore = Semaphore::Create({});
    Driver::DeletionQueue().AddDeleter([semaphore](){ Semaphore::Destroy(semaphore); });

    return semaphore;
}

Semaphore Semaphore::Builder::BuildManualLifetime()
{
    return Semaphore::Create({});
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

TimelineSemaphore TimelineSemaphore::Builder::Build()
{
    TimelineSemaphore semaphore = TimelineSemaphore::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([semaphore](){ TimelineSemaphore::Destroy(semaphore); });

    return semaphore;
}

TimelineSemaphore TimelineSemaphore::Builder::BuildManualLifetime()
{
    return TimelineSemaphore::Create(m_CreateInfo);
}

TimelineSemaphore::Builder& TimelineSemaphore::Builder::InitialValue(u64 value)
{
    m_CreateInfo.InitialValue = value;

    return *this;
}

TimelineSemaphore TimelineSemaphore::Create(const Builder::CreateInfo& createInfo)
{
    TimelineSemaphore semaphore = {};

    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = createInfo.InitialValue;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    vkCreateSemaphore(Driver::DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphore.m_Semaphore);

    return semaphore;
}

void TimelineSemaphore::Destroy(const TimelineSemaphore& semaphore)
{
    vkDestroySemaphore(Driver::DeviceHandle(), semaphore.m_Semaphore, nullptr);
}

void TimelineSemaphore::WaitCPU(u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_Semaphore;
    waitInfo.pValues = &value;
    
    VulkanCheck(vkWaitSemaphores(Driver::DeviceHandle(), &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void TimelineSemaphore::SignalCPU(u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_Semaphore;
    signalInfo.value = value;

    VulkanCheck(vkSignalSemaphore(Driver::DeviceHandle(), &signalInfo),
        "Failed to signal semaphore");

    m_Timeline = value;
}

DependencyInfo DependencyInfo::Builder::Build()
{
    return DependencyInfo::Create(m_CreateInfo);
}

DependencyInfo::Builder& DependencyInfo::Builder::SetFlags(VkDependencyFlags flags)
{
    m_CreateInfo.DependencyFlags = flags;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::ExecutionDependency(
    const ExecutionDependencyInfo& executionDependencyInfo)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = executionDependencyInfo.SourceStage;
    memoryBarrier.dstStageMask = executionDependencyInfo.DestinationStage;
    m_CreateInfo.ExecutionDependencyInfo = memoryBarrier;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::MemoryDependency(const MemoryDependencyInfo& memoryDependencyInfo)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = memoryDependencyInfo.SourceStage;
    memoryBarrier.dstStageMask = memoryDependencyInfo.DestinationStage;
    memoryBarrier.srcAccessMask = memoryDependencyInfo.SourceAccess;
    memoryBarrier.dstAccessMask = memoryDependencyInfo.DestinationAccess;
    m_CreateInfo.MemoryDependencyInfo = memoryBarrier;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::LayoutTransition(const LayoutTransitionInfo& layoutTransitionInfo)
{
    Driver::Unpack(layoutTransitionInfo, m_CreateInfo);

    return *this;
}

DependencyInfo DependencyInfo::Create(const Builder::CreateInfo& createInfo)
{
    DependencyInfo dependencyInfo = {};

    if (createInfo.ExecutionDependencyInfo.has_value())
        dependencyInfo.m_ExecutionMemoryDependenciesInfo.push_back(*createInfo.ExecutionDependencyInfo);
    if (createInfo.MemoryDependencyInfo.has_value())
        dependencyInfo.m_ExecutionMemoryDependenciesInfo.push_back(*createInfo.MemoryDependencyInfo);
    if (createInfo.LayoutTransitionInfo.has_value())
        dependencyInfo.m_LayoutTransitionInfo.push_back(*createInfo.LayoutTransitionInfo);

    dependencyInfo.m_DependencyInfo = {};
    dependencyInfo.m_DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.m_DependencyInfo.dependencyFlags = createInfo.DependencyFlags;

    return dependencyInfo;
}


void Barrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo)
{
    RenderCommand::WaitOnBarrier(cmd, dependencyInfo);
}

SplitBarrier SplitBarrier::Builder::Build()
{
    SplitBarrier splitBarrier = SplitBarrier::Create({});
    Driver::DeletionQueue().AddDeleter([splitBarrier](){ SplitBarrier::Destroy(splitBarrier); });

    return splitBarrier;
}

SplitBarrier SplitBarrier::Builder::BuildManualLifetime()
{
    return SplitBarrier::Create({});
}

SplitBarrier SplitBarrier::Create(const Builder::CreateInfo& createInfo)
{
    SplitBarrier splitBarrier = {};
    
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    VulkanCheck(vkCreateEvent(Driver::DeviceHandle(), &eventCreateInfo, nullptr, &splitBarrier.m_Event),
        "Failed to create split barrier");

    return splitBarrier;
}

void SplitBarrier::Destroy(const SplitBarrier& splitBarrier)
{
    vkDestroyEvent(Driver::DeviceHandle(), splitBarrier.m_Event, nullptr);
}

void SplitBarrier::Signal(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::SignalSplitBarrier(cmd, *this, dependencyInfo);
}

void SplitBarrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnSplitBarrier(cmd, *this, dependencyInfo);
}

void SplitBarrier::Reset(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::ResetSplitBarrier(cmd, *this, dependencyInfo);
}
