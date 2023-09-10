#pragma once
#include "CommandBuffer.h"
#include "Syncronization.h"

struct UploadContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    Fence Fence;
    QueueInfo QueueInfo;
};
