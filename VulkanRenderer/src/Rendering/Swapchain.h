#pragma once

#include <vector>
#include <glm/vec2.hpp>

#include "Image/Image.h"
#include "Synchronization.h"
#include "types.h"

class QueueInfo;
class ShaderPipelineTemplate;
struct DeviceQueues;

struct GLFWwindow;

struct SwapchainFrameSync
{
    Fence RenderFence;
    Semaphore RenderSemaphore;
    Semaphore PresentSemaphore;
};

static constexpr u32 INVALID_SWAPCHAIN_IMAGE = std::numeric_limits<u32>::max();

struct SwapchainCreateInfo
{
    glm::uvec2 DrawResolution{};
    Format DrawFormat{Format::RGBA16_FLOAT};
    Format DepthStencilFormat{Format::D32_FLOAT};
    Span<const SwapchainFrameSync> FrameSyncs{};
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
    std::vector<SwapchainFrameSync> Sync{};
};

struct SwapchainTag{};
using Swapchain = ResourceHandleType<SwapchainTag>;