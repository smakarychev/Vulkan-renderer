#pragma once

#include "types.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/DeletionQueue.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Commands/RenderCommandList.h"

class ResourceUploader;
class Camera;

struct FrameSync
{
    Fence RenderFence;
    Semaphore PresentSemaphore;
};

struct FrameContext
{
    u32 CommandBufferIndex{0};
    
    FrameSync FrameSync{};
    u32 FrameNumber{};
    u64 FrameNumberTick{};

    glm::uvec2 Resolution{};
    
    CommandBuffer Cmd{};
    RenderCommandList CommandList{};
    
    DeletionQueue DeletionQueue{};
    
    Camera* PrimaryCamera{nullptr};
    ResourceUploader* ResourceUploader{nullptr};
};
