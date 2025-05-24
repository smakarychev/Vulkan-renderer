#pragma once
#include "CommandBuffer.h"
#include "Descriptors.h"
#include "Pipeline.h"
#include "RenderingInfo.h"
#include "Swapchain.h"
#include "Buffer/Buffer.h"
#include "Buffer/BufferArena.h"

class DeletionQueue
{
    FRIEND_INTERNAL
public:
    ~DeletionQueue() { Flush(); }

    template <typename Type>
    void Enqueue(Type type);

    void Flush();
private:
    bool m_IsDummy{false};
    std::vector<Swapchain> m_Swapchains;
    std::vector<Buffer> m_Buffers;
    std::vector<BufferArena> m_BufferArenas;
    std::vector<Image> m_Images;
    std::vector<Sampler> m_Samplers;
    std::vector<CommandPool> m_CommandPools;
    std::vector<DescriptorsLayout> m_DescriptorLayouts;
    std::vector<DescriptorArenaAllocator> m_DescriptorArenaAllocators;
    std::vector<PipelineLayout> m_PipelineLayouts;
    std::vector<Pipeline> m_Pipelines;
    std::vector<ShaderModule> m_ShaderModules;
    std::vector<RenderingAttachment> m_RenderingAttachments;
    std::vector<RenderingInfo> m_RenderingInfos;
    std::vector<Fence> m_Fences;
    std::vector<Semaphore> m_Semaphores;
    std::vector<TimelineSemaphore> m_TimelineSemaphore;
    std::vector<DependencyInfo> m_DependencyInfos;
    std::vector<SplitBarrier> m_SplitBarriers;
};

template <typename Type>
void DeletionQueue::Enqueue(Type type)
{
    using Decayed = std::decay_t<Type>;
    
    if (m_IsDummy)
        return;
    
    if constexpr(std::is_same_v<Decayed, Swapchain>)
        m_Swapchains.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        m_Buffers.push_back(type);
    else if constexpr(std::is_same_v<Decayed, BufferArena>)
        m_BufferArenas.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Image>)
        m_Images.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        m_Samplers.push_back(type);
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        m_CommandPools.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        m_DescriptorLayouts.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocator>)
        m_DescriptorArenaAllocators.push_back(type);
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        m_PipelineLayouts.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        m_Pipelines.push_back(type);
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        m_ShaderModules.push_back(type);
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        m_RenderingAttachments.push_back(type);
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        m_RenderingInfos.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Fence>)
        m_Fences.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        m_Semaphores.push_back(type);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        m_TimelineSemaphore.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        m_DependencyInfos.push_back(type);
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        m_SplitBarriers.push_back(type);
    else 
        static_assert(!sizeof(Type), "No match for type");
}