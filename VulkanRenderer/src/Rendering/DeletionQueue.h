#pragma once

#include "Vulkan/Device.h"

class DeletionQueue
{
    FRIEND_INTERNAL
public:
    ~DeletionQueue() { Flush(); }

    template <typename Type>
    void Enqueue(Type type);

    void Flush();
private:
    using DeletionFunction = void (*)(u32);
    struct DeletionInfo
    {
        u32 Handle{};
        DeletionFunction DeletionFunction{};
    };
private:
    bool m_IsDummy{false};
    std::vector<DeletionInfo> m_DeletionInfos;
};

template <typename Type>
void DeletionQueue::Enqueue(Type type)
{
    using Decayed = std::decay_t<Type>;
    
    if (m_IsDummy)
        return;

    m_DeletionInfos.push_back({
        .Handle = type.m_Id, .DeletionFunction = [](u32 id) { Device::Destroy(Decayed(id)); }
    });
}