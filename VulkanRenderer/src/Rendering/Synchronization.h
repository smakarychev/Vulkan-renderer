#pragma once

#include <optional>

#include "RenderingCommon.h"

#include "ResourceHandle.h"
#include "Image/Image.h"
#include "Image/ImageTraits.h"
#include "SynchronizationTraits.h"

class DeletionQueue;
struct ImageSubresource;
struct BufferSubresource;
class CommandBuffer;

struct FenceCreateInfo
{
    bool IsSignaled{false};
};

class Fence
{
    FRIEND_INTERNAL
public:
    static void Destroy(const Fence& fence);

    void Reset() const;
    void Wait() const;
    bool Check() const;
private:
    ResourceHandleType<Fence> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<Fence> m_ResourceHandle{};
};

class Semaphore
{
    FRIEND_INTERNAL
public:
    static void Destroy(const Semaphore& semaphore);
private:
    ResourceHandleType<Semaphore> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<Semaphore> m_ResourceHandle{};
};

class TimelineSemaphore
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class TimelineSemaphore;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            u64 InitialValue{0};
        };
    public:
        TimelineSemaphore Build();
        TimelineSemaphore Build(DeletionQueue& deletionQueue);
        TimelineSemaphore BuildManualLifetime();
        Builder& InitialValue(u64 value);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static TimelineSemaphore Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const TimelineSemaphore& semaphore);
    
    void WaitCPU(u64 value) const;
    void SignalCPU(u64 value);

    u64 GetTimeline() const { return m_Timeline; }
    void SetTimeline(u64 timeline) { m_Timeline = timeline; }
private:
    ResourceHandleType<Semaphore> Handle() const { return m_ResourceHandle; }
private:
    u64 m_Timeline{0};
    ResourceHandleType<Semaphore> m_ResourceHandle;
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
    ImageSubresource ImageSubresource;
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
        friend class DependencyInfo;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            PipelineDependencyFlags Flags;
            std::optional<ExecutionDependencyInfo> ExecutionDependencyInfo;
            std::optional<MemoryDependencyInfo> MemoryDependencyInfo;
            std::optional<LayoutTransitionInfo> LayoutTransitionInfo;
        };
    public:
        DependencyInfo Build();
        DependencyInfo Build(DeletionQueue& deletionQueue);
        Builder& SetFlags(PipelineDependencyFlags flags);
        Builder& ExecutionDependency(const ExecutionDependencyInfo& executionDependencyInfo);
        Builder& MemoryDependency(const MemoryDependencyInfo& memoryDependencyInfo);
        Builder& LayoutTransition(const LayoutTransitionInfo& layoutTransitionInfo);
    private:
        CreateInfo m_CreateInfo{};
    };
public:
    static DependencyInfo Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DependencyInfo& dependencyInfo);
private:
    ResourceHandleType<DependencyInfo> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<DependencyInfo> m_ResourceHandle{};
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
        FRIEND_INTERNAL
        struct CreateInfo{};
    public:
        SplitBarrier Build();
        SplitBarrier Build(DeletionQueue& deletionQueue);
        SplitBarrier BuildManualLifetime();
    };
public:
    static SplitBarrier Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const SplitBarrier& splitBarrier);

    void Signal(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
    void Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
    void Reset(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const;
private:
    ResourceHandleType<SplitBarrier> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<SplitBarrier> m_ResourceHandle{};
};