#pragma once

#include "Vulkan/Device.h"

#include <mutex>

class DeletionQueue
{
    FRIEND_INTERNAL
public:
    ~DeletionQueue() { Flush(); }
    
    template <typename Type>
    void Enqueue(Type type);
    
    void Flush();
private:
    using DeletionFunction = void (*)(u64);
    struct DeletionInfo
    {
        u64 Handle{};
        DeletionFunction DeletionFunction{};
    };
private:
    bool m_IsDummy{false};
    std::vector<DeletionInfo> m_DeletionInfos;
    std::mutex m_Mutex{};
};

template <typename Type>
void DeletionQueue::Enqueue(Type type)
{
    using Decayed = std::decay_t<Type>;
    
    if (m_IsDummy)
        return;
    
    std::scoped_lock lock(m_Mutex);

    m_DeletionInfos.push_back({
        .Handle = type.GetId(), .DeletionFunction = [](u64 id)
        {
            Device::Destroy(Decayed(id));
        }
    });
}
