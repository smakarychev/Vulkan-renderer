#include "Synchronization.h"

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"

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
