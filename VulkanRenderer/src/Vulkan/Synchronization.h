#pragma once

#include <optional>

#include "VulkanCommon.h"

#include <Vulkan/vulkan_core.h>

#include "SynchronizationTraits.h"

struct ImageSubresource;
struct BufferSubresource;
class CommandBuffer;
class Device;

class Fence
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Fence;
        struct CreateInfo
        {
            VkFenceCreateFlags Flags;
        };
    public:
        Builder& StartSignaled(bool isSignaled = true);
        Fence Build();
        Fence BuildManualLifetime();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Fence Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Fence& semaphore);

    void Reset() const;
    void Wait() const;
    bool Check() const;
private:
    VkFence m_Fence{VK_NULL_HANDLE};
};

class Semaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Semaphore;
        struct CreateInfo{};
    public:
        Semaphore Build();
        Semaphore BuildManualLifetime();
    };
public:
    static Semaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Semaphore& semaphore);
private:
    VkSemaphore m_Semaphore{VK_NULL_HANDLE};
};

class TimelineSemaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class TimelineSemaphore;
        struct CreateInfo
        {
            u64 InitialValue{0};
        };
    public:
        TimelineSemaphore Build();
        TimelineSemaphore BuildManualLifetime();
        Builder& InitialValue(u64 value);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static TimelineSemaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const TimelineSemaphore& semaphore);
    
    void WaitCPU(u64 value);
    void SignalCPU(u64 value);

    u64 GetTimeline() const { return m_Timeline; }
    void SetTimeline(u64 timeline) { m_Timeline = timeline; }
    
private:
    VkSemaphore m_Semaphore{VK_NULL_HANDLE};
    u64 m_Timeline{0};
};

// todo: add queue transfer somewhere
struct ExecutionDependencyInfo
{
    PipelineStage SourceStage;
    PipelineStage DestinationStage;
};

struct MemoryDependencyInfo
{
    PipelineStage SourceStage;
    PipelineStage DestinationStage;
    PipelineAccess SourceAccess;   
    PipelineAccess DestinationAccess;
};

struct LayoutTransitionInfo
{
    const ImageSubresource* ImageSubresource;
    PipelineStage SourceStage;
    PipelineStage DestinationStage;
    PipelineAccess SourceAccess;   
    PipelineAccess DestinationAccess;
    ImageLayout OldLayout;
    ImageLayout NewLayout;
};

class DependencyInfo
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        FRIEND_INTERNAL
        friend class DependencyInfo;
        struct CreateInfo
        {
            VkDependencyFlags DependencyFlags;
            std::optional<VkMemoryBarrier2> ExecutionDependencyInfo;
            std::optional<VkMemoryBarrier2> MemoryDependencyInfo;
            std::optional<VkImageMemoryBarrier2> LayoutTransitionInfo;
        };
    public:
        DependencyInfo Build();
        Builder& SetFlags(PipelineDependencyFlags flags);
        Builder& ExecutionDependency(const ExecutionDependencyInfo& executionDependencyInfo);
        Builder& MemoryDependency(const MemoryDependencyInfo& memoryDependencyInfo);
        Builder& LayoutTransition(const LayoutTransitionInfo& layoutTransitionInfo);
    private:
        CreateInfo m_CreateInfo{};
    };
public:
    static DependencyInfo Create(const Builder::CreateInfo& createInfo);
private:
    VkDependencyInfo m_DependencyInfo;
    std::vector<VkMemoryBarrier2> m_ExecutionMemoryDependenciesInfo;
    std::vector<VkImageMemoryBarrier2> m_LayoutTransitionInfo;
};

class Barrier
{
public:
    void Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
};

class SplitBarrier
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class SplitBarrier;
        struct CreateInfo{};
    public:
        SplitBarrier Build();
        SplitBarrier BuildManualLifetime();
    };
public:
    static SplitBarrier Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const SplitBarrier& splitBarrier);

    void Signal(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
    void Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
    void Reset(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
private:
    VkEvent m_Event;
};