#include "rendererpch.h"

#include "Swapchain.h"

#include "Vulkan/Device.h"

u32 Swapchain::AcquireNextImage(Fence renderFence, Semaphore presentSemaphore) const
{
    return Device::AcquireNextImage(*this, renderFence, presentSemaphore);
}

bool Swapchain::Present(QueueKind queueKind, u32 imageIndex) const
{
    return Device::Present(*this, queueKind, imageIndex);
}

SwapchainDescription& Swapchain::GetDescription() const
{
    return Device::GetSwapchainDescription(*this);
}

Semaphore Swapchain::GetRenderSemaphore(u32 imageIndex) const
{
    return Device::GetSwapchainRenderSemaphore(*this, imageIndex);
}
