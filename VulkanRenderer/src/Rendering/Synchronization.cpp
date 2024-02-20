#include "Synchronization.h"

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

Fence Fence::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

Fence Fence::Builder::Build(DeletionQueue& deletionQueue)
{
    Fence fence = Fence::Create(m_CreateInfo);
    deletionQueue.AddDeleter([fence](){ Fence::Destroy(fence); });

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
    return Driver::Create(createInfo);
}

void Fence::Destroy(const Fence& fence)
{
    Driver::Destroy(fence);
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
    return RenderCommand::CheckFence(*this);
}

Semaphore Semaphore::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

Semaphore Semaphore::Builder::Build(DeletionQueue& deletionQueue)
{
    Semaphore semaphore = Semaphore::Create({});
    deletionQueue.AddDeleter([semaphore](){ Semaphore::Destroy(semaphore); });

    return semaphore;
}

Semaphore Semaphore::Builder::BuildManualLifetime()
{
    return Semaphore::Create({});
}

Semaphore Semaphore::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void Semaphore::Destroy(const Semaphore& semaphore)
{
    Driver::Destroy(semaphore);
}

TimelineSemaphore TimelineSemaphore::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

TimelineSemaphore TimelineSemaphore::Builder::Build(DeletionQueue& deletionQueue)
{
    TimelineSemaphore semaphore = TimelineSemaphore::Create(m_CreateInfo);
    deletionQueue.AddDeleter([semaphore](){ TimelineSemaphore::Destroy(semaphore); });

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
    return Driver::Create(createInfo);
}

void TimelineSemaphore::Destroy(const TimelineSemaphore& semaphore)
{
    Driver::Destroy(semaphore);
}

void TimelineSemaphore::WaitCPU(u64 value) const
{
    Driver::TimelineSemaphoreWaitCPU(*this, value);
}

void TimelineSemaphore::SignalCPU(u64 value)
{
    Driver::TimelineSemaphoreSignalCPU(*this, value);
}


DependencyInfo DependencyInfo::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

DependencyInfo DependencyInfo::Builder::Build(DeletionQueue& deletionQueue)
{
    DependencyInfo dependencyInfo = DependencyInfo::Create(m_CreateInfo);
    deletionQueue.AddDeleter([dependencyInfo]() { DependencyInfo::Destroy(dependencyInfo); });

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
    return Driver::Create(createInfo);
}

void DependencyInfo::Destroy(const DependencyInfo& dependencyInfo)
{
    Driver::Destroy(dependencyInfo);
}


void Barrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnBarrier(cmd, dependencyInfo);
}

SplitBarrier SplitBarrier::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

SplitBarrier SplitBarrier::Builder::Build(DeletionQueue& deletionQueue)
{
    SplitBarrier splitBarrier = SplitBarrier::Create({});
    deletionQueue.AddDeleter([splitBarrier](){ SplitBarrier::Destroy(splitBarrier); });

    return splitBarrier;
}

SplitBarrier SplitBarrier::Builder::BuildManualLifetime()
{
    return SplitBarrier::Create({});
}

SplitBarrier SplitBarrier::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void SplitBarrier::Destroy(const SplitBarrier& splitBarrier)
{
    Driver::Destroy(splitBarrier);
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
