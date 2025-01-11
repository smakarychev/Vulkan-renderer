#pragma once

#include "RGResource.h"
#include "Rendering/Shader/Shader.h"

class Camera;
class CommandBuffer;

namespace RG
{
    struct GlobalResources
    {
        u64 FrameNumberTick{0};
        glm::uvec2 Resolution{};
        const Camera* PrimaryCamera{nullptr}; 
        Resource PrimaryCameraGPU{};
        Resource ShadingSettings{};
    };
}



