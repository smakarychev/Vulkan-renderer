#pragma once

#include "types.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/Swapchain.h"
#include "Vulkan/Device.h"

class ResourceUploader;
class Camera;

struct FrameContext
{
    u32 CommandBufferIndex{0};
    
    SwapchainFrameSync FrameSync;
    u32 FrameNumber;
    u64 FrameNumberTick;

    glm::uvec2 Resolution{};
    
    CommandBuffer Cmd;
    
    DeletionQueue DeletionQueue;
    
    Camera* PrimaryCamera{nullptr};
    ResourceUploader* ResourceUploader{nullptr};
};
