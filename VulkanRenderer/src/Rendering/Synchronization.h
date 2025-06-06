#pragma once

#include <optional>

#include "ResourceHandle.h"
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
using Fence = ResourceHandleType<FenceTag>;

struct SemaphoreTag{};
using Semaphore = ResourceHandleType<SemaphoreTag>;

struct TimelineSemaphoreCreateInfo
{
    u64 InitialValue{0};
};

struct TimelineSemaphoreTag{};
using TimelineSemaphore = ResourceHandleType<TimelineSemaphoreTag>;

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

struct DependencyInfoCreateInfo
{
    PipelineDependencyFlags Flags{PipelineDependencyFlags::None};
    std::optional<ExecutionDependencyInfo> ExecutionDependencyInfo{};
    std::optional<MemoryDependencyInfo> MemoryDependencyInfo{};
    std::optional<LayoutTransitionInfo> LayoutTransitionInfo{};
};

struct DependencyInfoTag{};
using DependencyInfo = ResourceHandleType<DependencyInfoTag>;

struct SplitBarrierTag{};
using SplitBarrier = ResourceHandleType<SplitBarrierTag>;