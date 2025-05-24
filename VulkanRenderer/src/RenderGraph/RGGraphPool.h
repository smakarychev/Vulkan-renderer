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

        template <typename Res>
        struct AllocationInfo
        {
            Res Resource;
            RG::Resource AliasedFrom{};
        };
        using BufferAllocationInfo = AllocationInfo<Buffer>;
        using ImageAllocationInfo = AllocationInfo<Image>;
        
        BufferAllocationInfo Allocate(Resource resource, const BufferDescription& buffer,
            u32 firstPassIndex, u32 lastPassIndex);
        ImageAllocationInfo Allocate(Resource resource, const ImageDescription& image,
            u32 firstPassIndex, u32 lastPassIndex);

        void OnFrameEnd();
    private:
        void ClearUnreferenced();

        BufferAllocationInfo TryAlias(Resource resource, const BufferDescription& buffer,
            u32 firstPassIndex, u32 lastPassIndex);
        ImageAllocationInfo TryAlias(Resource resource, const ImageDescription& image,
            u32 firstPassIndex, u32 lastPassIndex);
        Buffer AllocateNew(Resource resource, const BufferDescription& buffer,
            u32 firstPassIndex, u32 lastPassIndex);
        Image AllocateNew(Resource resource, const ImageDescription& image,
            u32 firstPassIndex, u32 lastPassIndex);
    private:
        template <typename Res, typename Desc>
        struct PoolResource
        {
            static constexpr u32 NO_PASS{~0u};
            Res Resource{};
            Desc Description{};
            RG::Resource Handle{};
            u32 FirstPassIndex{NO_PASS};
            u32 LastPassIndex{NO_PASS};
            /* allocated resources are freed, if not used for some number of frames */
            u32 LastFrameIndex{0};
        };

        std::vector<PoolResource<Buffer, BufferDescription>> m_Buffers;
        std::vector<PoolResource<Image, ImageDescription>> m_Images;
    };    
}


