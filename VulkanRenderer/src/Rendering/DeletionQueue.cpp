#include "rendererpch.h"

#include "DeletionQueue.h"

void DeletionQueue::Flush()
{
    std::scoped_lock lock(m_Mutex);
    
    for (auto& deletion : m_DeletionInfos)
        deletion.DeletionFunction(deletion.Handle);
    
    m_DeletionInfos.clear();
}
