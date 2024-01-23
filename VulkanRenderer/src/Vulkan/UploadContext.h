#pragma once
#include "CommandBuffer.h"
#include "Synchronization.h"

struct UploadContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    Fence Fence;
    QueueInfo QueueInfo;
};
