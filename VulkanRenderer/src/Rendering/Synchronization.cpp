#include "Synchronization.h"

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"

Fence Fence::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

Fence Fence::Builder::Build(DeletionQueue& deletionQueue)
{
    Fence fence = Fence::Create(m_CreateInfo);
    deletionQueue.Enqueue(fence);

    return fence;
}

Fence Fence::Builder::BuildManualLifetime()
{
    return Fence::Create(m_CreateInfo);
}

Fence::Builder& Fence::Builder::StartSignaled(bool isSignaled)
{
    m_CreateInfo.IsSignaled = isSignaled;
    
    return *this;
}

Fence Fence::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void Fence::Destroy(const Fence& fence)
{
    Device::Destroy(fence.Handle());
}

void Fence::Reset() const
{
    Device::ResetFence(*this);
}

void Fence::Wait() const
{
    Device::WaitForFence(*this);
}

bool Fence::Check() const
{
    return Device::CheckFence(*this);
}

Semaphore Semaphore::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

Semaphore Semaphore::Builder::Build(DeletionQueue& deletionQueue)
{
    Semaphore semaphore = Semaphore::Create({});
    deletionQueue.Enqueue(semaphore);

    return semaphore;
}

Semaphore Semaphore::Builder::BuildManualLifetime()
{
    return Semaphore::Create({});
}

Semaphore Semaphore::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void Semaphore::Destroy(const Semaphore& semaphore)
{
    Device::Destroy(semaphore.Handle());
}

TimelineSemaphore TimelineSemaphore::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

TimelineSemaphore TimelineSemaphore::Builder::Build(DeletionQueue& deletionQueue)
{
    TimelineSemaphore semaphore = TimelineSemaphore::Create(m_CreateInfo);
    deletionQueue.Enqueue(semaphore);

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
    return Device::Create(createInfo);
}

void TimelineSemaphore::Destroy(const TimelineSemaphore& semaphore)
{
    Device::Destroy(semaphore.Handle());
}

void TimelineSemaphore::WaitCPU(u64 value) const
{
    Device::TimelineSemaphoreWaitCPU(*this, value);
}

void TimelineSemaphore::SignalCPU(u64 value)
{
    Device::TimelineSemaphoreSignalCPU(*this, value);
}


DependencyInfo DependencyInfo::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

DependencyInfo DependencyInfo::Builder::Build(DeletionQueue& deletionQueue)
{
    DependencyInfo dependencyInfo = DependencyInfo::Create(m_CreateInfo);
    deletionQueue.Enqueue(dependencyInfo);

    return dependencyInfo;
}

DependencyInfo::Builder& DependencyInfo::Builder::SetFlags(PipelineDependencyFlags flags)
{
    m_CreateInfo.Flags = flags;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::ExecutionDependency(
    const ExecutionDependencyInfo& executionDependencyInfo)
{
    m_CreateInfo.ExecutionDependencyInfo = executionDependencyInfo;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::MemoryDependency(const MemoryDependencyInfo& memoryDependencyInfo)
{
    m_CreateInfo.MemoryDependencyInfo = memoryDependencyInfo;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::LayoutTransition(const LayoutTransitionInfo& layoutTransitionInfo)
{
    m_CreateInfo.LayoutTransitionInfo = layoutTransitionInfo;

    return *this;
}

DependencyInfo DependencyInfo::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void DependencyInfo::Destroy(const DependencyInfo& dependencyInfo)
{
    Device::Destroy(dependencyInfo.Handle());
}


void Barrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnBarrier(cmd, dependencyInfo);
}

SplitBarrier SplitBarrier::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

SplitBarrier SplitBarrier::Builder::Build(DeletionQueue& deletionQueue)
{
    SplitBarrier splitBarrier = SplitBarrier::Create({});
    deletionQueue.Enqueue(splitBarrier);

    return splitBarrier;
}

SplitBarrier SplitBarrier::Builder::BuildManualLifetime()
{
    return SplitBarrier::Create({});
}

SplitBarrier SplitBarrier::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void SplitBarrier::Destroy(const SplitBarrier& splitBarrier)
{
    Device::Destroy(splitBarrier.Handle());
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
