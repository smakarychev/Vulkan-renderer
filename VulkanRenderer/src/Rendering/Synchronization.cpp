#include "rendererpch.h"

#include "Synchronization.h"

#include "Vulkan/Device.h"

void Fence::Wait() const
{
    Device::WaitForFence(*this);
}

bool Fence::Check() const
{
    return Device::CheckFence(*this);
}

void Fence::Reset() const
{
    Device::ResetFence(*this);
}

void TimelineSemaphore::WaitCPU(u64 value) const
{
    Device::TimelineSemaphoreWaitCPU(*this, value);
}

void TimelineSemaphore::SignalCPU(u64 value) const
{
    Device::TimelineSemaphoreSignalCPU(*this, value);
}
