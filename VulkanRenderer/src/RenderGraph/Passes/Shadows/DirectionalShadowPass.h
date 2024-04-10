#pragma once

#include "RenderGraph/Passes/Culling/CullMetaPass.h"

class DirectionalShadowPass
{
public:
    struct PassData
    {
        RG::Resource ShadowOut{};

        const Camera* ShadowCamera{nullptr}; 
    };
public:
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
};
