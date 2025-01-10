#pragma once

#include "ResourceHandle.h"

class ShaderModule
{
    FRIEND_INTERNAL
public:
    ResourceHandleType<ShaderModule> Handle() const { return m_ResourceHandle; }
private:
    // todo: change once handles are ready
    ResourceHandleType<ShaderModule> m_ResourceHandle{};
};
