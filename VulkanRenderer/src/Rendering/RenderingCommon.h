#pragma once

#include "types.h"

#define FRIEND_INTERNAL \
    friend class Device; \
    friend class DeviceResources; \
    friend class DeletionQueue; \

// todo: cleanup this entire file

enum class QueueKind {Graphics, Presentation, Compute};

struct DepthBias
{
    f32 Constant{0.0f};
    f32 Slope{0.0f};
};