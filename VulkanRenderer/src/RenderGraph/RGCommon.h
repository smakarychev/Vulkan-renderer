#pragma once

#include "RGResource.h"
#include "ViewInfoGPU.h"
#include "Rendering/Shader/Shader.h"

class Camera;

namespace RG
{
    struct GlobalResources
    {
        u64 FrameNumberTick{0};
        glm::uvec2 Resolution{};
        const Camera* PrimaryCamera{nullptr}; 
        ViewInfoGPU PrimaryViewInfo{};
        Resource PrimaryViewInfoResource{};
    };
}



