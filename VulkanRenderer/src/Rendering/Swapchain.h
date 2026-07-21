#pragma once

#include "Synchronization.h"
#include "Image/Image.h"

#include <CoreLib/types.h>

#include <vector>
#include <glm/vec2.hpp>

static constexpr u32 INVALID_SWAPCHAIN_IMAGE = std::numeric_limits<u32>::max();

struct SwapchainCreateInfo
{
    glm::uvec2 DrawResolution{};
    Format DrawFormat{Format::RGBA16_FLOAT};
    Format DepthStencilFormat{Format::D32_FLOAT};
};

struct SwapchainDescription
{
    glm::uvec2 SwapchainResolution{};
    glm::uvec2 DrawResolution{};
    Format DrawFormat{};
    Format DepthFormat{};
    std::vector<Image> ColorImages{};
    Image DrawImage{};
    Image DepthImage{};
};

struct SwapchainTag{};
struct Swapchain : ResourceHandleType<SwapchainTag>
{
    u32 AcquireNextImage(Fence renderFence, Semaphore presentSemaphore) const;
    bool Present(QueueKind queueKind, u32 imageIndex) const;
    SwapchainDescription& GetDescription() const;
    Semaphore GetRenderSemaphore(u32 imageIndex) const;
};