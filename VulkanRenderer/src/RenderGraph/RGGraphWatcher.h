#pragma once

#include "RGResource.h"

#include <CoreLib/types.h>

#include <memory>
#include <vector>

struct DependencyInfoCreateInfo;

namespace RG
{
struct ImageResourceAccess;
struct BufferResourceAccess;
struct RGBuffer;
struct RGImage;
class Pass;

class GraphWatcher
{
public:
    GraphWatcher() = default;
    GraphWatcher(const GraphWatcher&) = delete;
    GraphWatcher& operator=(const GraphWatcher&) = delete;
    GraphWatcher(GraphWatcher&&) = delete;
    GraphWatcher& operator=(GraphWatcher&&) = delete;
    virtual ~GraphWatcher() = default;

    virtual void OnPassOrderFinalized(const std::vector<std::unique_ptr<Pass>>& passes)
    {
    }

    virtual void OnBufferResourcesFinalized(const std::vector<RGBuffer>& buffers)
    {
    }

    virtual void OnImageResourcesFinalized(const std::vector<RGImage>& images)
    {
    }

    virtual void OnBufferAccessesFinalized(const std::vector<BufferResourceAccess>& accesses)
    {
    }

    virtual void OnImagesAccessesFinalized(const std::vector<ImageResourceAccess>& accesses)
    {
    }

    struct BarrierInfo
    {
        enum class Type : u8
        {
            Barrier,
            SplitBarrier,
        };

        Type BarrierType{Type::Barrier};
        const DependencyInfoCreateInfo* DependencyInfo{nullptr};
    };
    struct BufferBarrier
    {
        BarrierInfo Info{};
        BufferResource Resource{};
    };
    struct ImageBarrier
    {
        BarrierInfo Info{};
        ImageResource Resource{};
    };

    virtual void OnBarrierAdded(const BufferBarrier& barrierInfo, const Pass& firstPass, const Pass& secondPass)
    {
    }

    virtual void OnBarrierAdded(const ImageBarrier& barrierInfo, const Pass& firstPass, const Pass& secondPass)
    {
    }

    virtual void OnReset()
    {
    }

protected:
    static u32 GetResourceIndex(const ResourceHandleBase& resource);
};

inline u32 GraphWatcher::GetResourceIndex(const ResourceHandleBase& resource)
{
    return resource.m_Index;
}
}
