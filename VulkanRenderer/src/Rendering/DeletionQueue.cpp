#include "rendererpch.h"

#include "DeletionQueue.h"

void DeletionQueue::Flush()
{
    for (auto& deletion : m_DeletionInfos)
        deletion.DeletionFunction(deletion.Handle);
    
    m_DeletionInfos.clear();
}
