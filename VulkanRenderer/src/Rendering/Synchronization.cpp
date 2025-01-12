#include "Synchronization.h"

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"

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

void Semaphore::Destroy(const Semaphore& semaphore)
{
    Device::Destroy(semaphore.Handle());
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

void DependencyInfo::Destroy(const DependencyInfo& dependencyInfo)
{
    Device::Destroy(dependencyInfo.Handle());
}


void Barrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnBarrier(cmd, dependencyInfo);
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
