#pragma once

#include <optional>

#include "Vulkan/DeviceResourceListHandle.h"
#include "Image/Image.h"
#include "Image/ImageTraits.h"
#include "SynchronizationTraits.h"

class DeletionQueue;
struct ImageSubresource;
struct BufferSubresource;

struct FenceCreateInfo
{
    bool IsSignaled{false};
};

struct FenceTag{};
struct Fence : lux::DeviceResourceListHandle<FenceTag>
{
    void Wait() const;
    bool Check() const;
    void Reset() const;
};

struct SemaphoreTag{};
using Semaphore = lux::DeviceResourceListHandle<SemaphoreTag>;

struct TimelineSemaphoreCreateInfo
{
    u64 InitialValue{0};
};

struct TimelineSemaphoreTag{};
struct TimelineSemaphore : lux::DeviceResourceListHandle<TimelineSemaphoreTag>
{
    void WaitCPU(u64 value) const;
    void SignalCPU(u64 value) const;
};

// todo: add queue transfer somewhere
struct ExecutionDependencyInfo
{
    PipelineStage SourceStage{PipelineStage::None};
    PipelineStage DestinationStage{PipelineStage::None};
};

struct MemoryDependencyInfo
{
    PipelineStage SourceStage{PipelineStage::None};
    PipelineStage DestinationStage{PipelineStage::None};
    PipelineAccess SourceAccess{PipelineAccess::None};   
    PipelineAccess DestinationAccess{PipelineAccess::None};
};

struct LayoutTransitionInfo
{
    ImageSubresource ImageSubresource{};
    PipelineStage SourceStage{PipelineStage::None};
    PipelineStage DestinationStage{PipelineStage::None};
    PipelineAccess SourceAccess{PipelineAccess::None};   
    PipelineAccess DestinationAccess{PipelineAccess::None};
    ImageLayout OldLayout{ImageLayout::Undefined};
    ImageLayout NewLayout{ImageLayout::Undefined};
};

enum class DependencyInfoType : u8
{
    Execution, Memory, Layout
};

struct DependencyInfo
{
    DependencyInfoType Type{DependencyInfoType::Execution};
    union
    {
        ExecutionDependencyInfo Execution{};
        MemoryDependencyInfo Memory;
        LayoutTransitionInfo Layout;
    } Dependency{};
    
    DependencyInfo() = default;
    DependencyInfo(const ExecutionDependencyInfo& dependency) : Type(DependencyInfoType::Execution)
    {
        Dependency.Execution = dependency;
    }
    DependencyInfo(const MemoryDependencyInfo& dependency) : Type(DependencyInfoType::Memory)
    {
        Dependency.Memory = dependency;
    }
    DependencyInfo(const LayoutTransitionInfo& dependency) : Type(DependencyInfoType::Layout)
    {
        Dependency.Layout = dependency;
    }
};

struct SplitBarrierTag{};
using SplitBarrier = lux::DeviceResourceListHandle<SplitBarrierTag>;