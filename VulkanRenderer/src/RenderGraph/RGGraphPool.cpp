#include "RGGraphPool.h"

#include "Settings.h"
#include "cvars/CVarSystem.h"
#include "Vulkan/Device.h"

#include <algorithm>

namespace RG
{
    namespace
    {
        bool canAliasBuffer(const BufferDescription& description, const BufferDescription& other,
            ResourceFlags otherFlags, u32 otherFrame)
        {
            bool canAlias = (otherFrame >= BUFFERED_FRAMES || 
                !enumHasAny(description.Usage, BufferUsage::Readback) &&
                !enumHasAny(otherFlags, ResourceFlags::Volatile)) &&
                description.SizeBytes == other.SizeBytes && description.Usage == other.Usage;
            
            return canAlias;
        }
        bool canAliasImage(const ImageDescription& description, const ImageDescription& other,
            ResourceFlags otherFlags, u32 otherFrame)
        {
            bool canAlias = (otherFrame >= BUFFERED_FRAMES || 
                !enumHasAny(otherFlags, ResourceFlags::Volatile)) &&
                description.Height == other.Height &&
                description.Width == other.Width &&
                description.LayersDepth == other.LayersDepth &&
                description.Mipmaps == other.Mipmaps &&
                description.Format == other.Format &&
                description.Kind == other.Kind &&
                description.Usage == other.Usage &&
                description.MipmapFilter == other.MipmapFilter;
            if (!canAlias)
                return false;

            if (description.AdditionalViews.size() != other.AdditionalViews.size())
                return false;

            for (u32 view = 0; view < description.AdditionalViews.size(); view++)
                if (description.AdditionalViews[view] != other.AdditionalViews[view])
                    return false;

            return true;
        }
    }

    GraphPool::~GraphPool()
    {
        for (auto& buffer : m_Buffers)
            Device::Destroy(buffer.Resource);
        for (auto& image : m_Images)
            Device::Destroy(image.Resource);
    }

    GraphPool::BufferAllocationInfo GraphPool::Allocate(Resource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        const BufferAllocationInfo allocated = TryAlias(resource, buffer, firstPassIndex, lastPassIndex);

        return allocated.Resource.HasValue() ?
            allocated : BufferAllocationInfo{.Resource = AllocateNew(resource, buffer, firstPassIndex, lastPassIndex)};
    }

    GraphPool::ImageAllocationInfo GraphPool::Allocate(Resource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        const ImageAllocationInfo allocated = TryAlias(resource, image, firstPassIndex, lastPassIndex);

        return allocated.Resource.HasValue() ?
            allocated : ImageAllocationInfo{.Resource = AllocateNew(resource, image, firstPassIndex, lastPassIndex)};
    }

    void GraphPool::OnFrameEnd()
    {
        for (auto& buffer : m_Buffers)
        {
            buffer.LastFrameIndex++;
            buffer.FirstPassIndex = buffer.NO_PASS;
            buffer.LastPassIndex = buffer.NO_PASS;
            buffer.Handle = {};
        }
        for (auto& image : m_Images)
        {
            image.LastFrameIndex++;
            image.FirstPassIndex = image.NO_PASS; 
            image.LastPassIndex = image.NO_PASS;
            image.Handle = {};
        }
        
        ClearUnreferenced();
    }

    void GraphPool::ClearUnreferenced()
    {
        auto unorderedRemove = [this](auto& collection, auto&& onDeleteFn)
        {
            auto toRemoveIt = std::ranges::partition(collection,
                [](auto& r)
                {
                    return r.LastFrameIndex < (u32)*CVars::Get().GetI32CVar("RG.UnreferencedResourcesLifetime"_hsv);
                }).begin();

            for (auto it = toRemoveIt; it != collection.end(); it++)
                onDeleteFn(it->Resource);
            collection.erase(toRemoveIt, collection.end());
        };

        unorderedRemove(m_Buffers, [](Buffer buffer) { Device::Destroy(buffer); });
        unorderedRemove(m_Images, [](Image image) { Device::Destroy(image); });
    }

    GraphPool::BufferAllocationInfo GraphPool::TryAlias(Resource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        for (auto& allocated : m_Buffers)
        {
            if ((allocated.FirstPassIndex == allocated.NO_PASS ||
                firstPassIndex > allocated.LastPassIndex || lastPassIndex < allocated.FirstPassIndex) &&
                canAliasBuffer(
                    buffer,
                    allocated.Description,
                    allocated.Handle.GetFlags(),
                    allocated.LastFrameIndex))
            {
                allocated.FirstPassIndex = firstPassIndex;
                allocated.LastPassIndex = lastPassIndex;
                allocated.LastFrameIndex = 0;
                const Resource aliasedFrom = allocated.Handle;
                allocated.Handle = resource;
                
                return {
                    .Resource = allocated.Resource,
                    .AliasedFrom = aliasedFrom
                };
            }
        }

        return {};
    }

    GraphPool::ImageAllocationInfo GraphPool::TryAlias(Resource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        for (auto& allocated : m_Images)
        {
            if ((allocated.FirstPassIndex == allocated.NO_PASS ||
                firstPassIndex > allocated.LastPassIndex || lastPassIndex < allocated.FirstPassIndex) &&
                canAliasImage(
                    image,
                    allocated.Description,
                    allocated.Handle.GetFlags(),
                    allocated.LastFrameIndex))
            {
                allocated.FirstPassIndex = firstPassIndex;
                allocated.LastPassIndex = lastPassIndex;
                allocated.LastFrameIndex = 0;
                const Resource aliasedFrom = allocated.Handle;
                allocated.Handle = resource;
                
                return {
                    .Resource = allocated.Resource,
                    .AliasedFrom = aliasedFrom
                };
            }
        }

        return {};
    }

    Buffer GraphPool::AllocateNew(Resource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        m_Buffers.push_back({
            .Resource = Device::CreateBuffer({
                .SizeBytes = buffer.SizeBytes,
                .Usage = buffer.Usage
            }, Device::DummyDeletionQueue()),
            .Description = buffer,
            .Handle = resource,
            .FirstPassIndex = firstPassIndex,
            .LastPassIndex = lastPassIndex,
            .LastFrameIndex = 0});

        return m_Buffers.back().Resource;
    }

    Image GraphPool::AllocateNew(Resource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex)
    {
        m_Images.push_back({
            .Resource = Device::CreateImage({
                .Description = image
            }, Device::DummyDeletionQueue()),
            .Description = image,
            .Handle = resource,
            .FirstPassIndex = firstPassIndex,
            .LastPassIndex = lastPassIndex,
            .LastFrameIndex = 0});

        return m_Images.back().Resource;
    }
}
