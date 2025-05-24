#pragma once

#include "types.h"
#include "RGResource.h"

#include <memory>
#include <vector>

struct DependencyInfoCreateInfo;

namespace RG
{
    struct BufferResource;
    struct ImageResource;
    struct ResourceAccess;
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

        virtual void OnPassOrderFinalized(const std::vector<std::unique_ptr<Pass>>& passes) {}
        virtual void OnBufferResourcesFinalized(const std::vector<BufferResource>& buffers) {}
        virtual void OnImageResourcesFinalized(const std::vector<ImageResource>& images) {}
        virtual void OnBufferAccessesFinalized(const std::vector<ResourceAccess>& accesses) {}
        virtual void OnImagesAccessesFinalized(const std::vector<ResourceAccess>& accesses) {}
        struct BarrierInfo
        {
            enum class Type : u8
            {
                Barrier,
                SplitBarrier,
            };
            Type BarrierType{Type::Barrier};
            Resource Resource{};
            const DependencyInfoCreateInfo* DependencyInfo{nullptr};
        };
        virtual void OnBarrierAdded(const BarrierInfo& barrierInfo, const Pass& firstPass, const Pass& secondPass) {}
        virtual void OnReset() {}
    protected:
        static u32 GetResourceIndex(Resource resource);
    };

    inline u32 GraphWatcher::GetResourceIndex(Resource resource)
    {
        return resource.m_Index;
    }
}
