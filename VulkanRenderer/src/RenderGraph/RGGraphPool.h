#pragma once

#include "RGResource.h"
#include "Rendering/Buffer/Buffer.h"
#include "Rendering/Image/Image.h"

namespace RG
{
class GraphPool
{
public:
    GraphPool() = default;
    GraphPool(const GraphPool&) = delete;
    GraphPool& operator=(const GraphPool&) = delete;
    GraphPool(GraphPool&&) = delete;
    GraphPool& operator=(GraphPool&&) = delete;
    ~GraphPool();

    template <typename Res, typename RGRes>
    struct AllocationInfo
    {
        Res Resource;
        RGRes AliasedFrom{};
    };

    using BufferAllocationInfo = AllocationInfo<Buffer, BufferResource>;
    using ImageAllocationInfo = AllocationInfo<Image, ImageResource>;

    BufferAllocationInfo Allocate(BufferResource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex);
    ImageAllocationInfo Allocate(ImageResource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex);

    void OnFrameEnd();

private:
    void ClearUnreferenced();

    BufferAllocationInfo TryAlias(BufferResource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex);
    ImageAllocationInfo TryAlias(ImageResource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex);
    Buffer AllocateNew(BufferResource resource, const BufferDescription& buffer,
        u32 firstPassIndex, u32 lastPassIndex);
    Image AllocateNew(ImageResource resource, const ImageDescription& image,
        u32 firstPassIndex, u32 lastPassIndex);

private:
    template <typename Res, typename RGRes, typename Desc>
    struct PoolResource
    {
        static constexpr u32 NO_PASS{~0u};
        Res Resource{};
        Desc Description{};
        RGRes Handle{};
        u32 FirstPassIndex{NO_PASS};
        u32 LastPassIndex{NO_PASS};
        /* allocated resources are freed, if not used for some number of frames */
        u32 LastFrameIndex{0};
    };

    std::vector<PoolResource<Buffer, BufferResource, BufferDescription>> m_Buffers;
    std::vector<PoolResource<Image, ImageResource, ImageDescription>> m_Images;
};
}
