#include "rendererpch.h"

#include "DeletionQueue.h"

#include "Vulkan/Device.h"

void DeletionQueue::Flush()
{
    for (auto handle : m_Buffers)
        Device::Destroy(handle);
    for (auto handle : m_BufferArenas)
        Device::Destroy(handle);
    for (auto handle : m_Images)
        Device::Destroy(handle);
    for (auto handle : m_Samplers)
        Device::Destroy(handle);
    
    for (auto handle : m_CommandPools)
        Device::Destroy(handle);
    
    for (auto handle : m_DescriptorLayouts)
        Device::Destroy(handle);
    
    for (auto handle : m_DescriptorArenaAllocators)
        Device::Destroy(handle);
    
    for (auto handle : m_PipelineLayouts)
        Device::Destroy(handle);
    for (auto handle : m_Pipelines)
        Device::Destroy(handle);
    for (auto handle : m_ShaderModules)
        Device::Destroy(handle);
    
    for (auto handle : m_RenderingAttachments)
        Device::Destroy(handle);
    for (auto handle : m_RenderingInfos)
        Device::Destroy(handle);
    
    for (auto handle : m_Fences)
        Device::Destroy(handle);
    for (auto handle : m_Semaphores)
        Device::Destroy(handle);
    for (auto handle : m_TimelineSemaphore)
        Device::Destroy(handle);
    for (auto handle : m_DependencyInfos)
        Device::Destroy(handle);
    for (auto handle : m_SplitBarriers)
        Device::Destroy(handle);
    
    for (auto handle : m_Swapchains)
        Device::Destroy(handle);
    
    m_Swapchains.clear();
    m_Buffers.clear();
    m_BufferArenas.clear();
    m_Images.clear();
    m_Samplers.clear();
    m_CommandPools.clear();
    m_DescriptorLayouts.clear();
    m_DescriptorArenaAllocators.clear();
    m_PipelineLayouts.clear();
    m_Pipelines.clear();
    m_ShaderModules.clear();
    m_RenderingAttachments.clear();
    m_RenderingInfos.clear();
    m_Fences.clear();
    m_Semaphores.clear();
    m_DependencyInfos.clear();
    m_SplitBarriers.clear();
}
