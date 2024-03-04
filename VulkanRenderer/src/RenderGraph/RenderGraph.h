#pragma once

#include <memory>
#include <vector>

#include "RenderGraphResource.h"
#include "RenderPass.h"
#include "Vulkan/Driver.h"

namespace RenderGraph
{
    enum class ResourceAccessFlags
    {
        None = 0,

        Vertex      = BIT(1),
        Pixel       = BIT(2),
        Compute     = BIT(3),

        Index       = BIT(4),
        Indirect    = BIT(5),
        Conditional = BIT(6),
        
        Attribute   = BIT(7),
        Uniform     = BIT(8),
        Storage     = BIT(9),

        Sampled     = BIT(10),
    };
    CREATE_ENUM_FLAGS_OPERATORS(ResourceAccessFlags)

    template <typename T>
    struct ResourceAliasTraits {};
        
    template <>
    struct ResourceAliasTraits<Buffer>
    {
        using ResourceTraits = ResourceTraits<Buffer>;
        static bool CanAlias(const ResourceTraits::Desc& description, const ResourceTraits::Desc& other)
        {
            return description.SizeBytes == other.SizeBytes && description.Usage == other.Usage;
        }
    };

    template <>
    struct ResourceAliasTraits<Texture>
    {
        using ResourceTraits = ResourceTraits<Texture>;
        static bool CanAlias(const ResourceTraits::Desc& description, const ResourceTraits::Desc& other)
        {
            return
                description.Height == other.Height &&
                description.Width == other.Width &&
                description.Layers == other.Layers &&
                description.Mipmaps == other.Mipmaps &&
                description.Views == other.Views &&
                description.Format == other.Format &&
                description.Kind == other.Kind &&
                description.Usage == other.Usage &&
                description.MipmapFilter == other.MipmapFilter;
        }
    };

    class RenderGraphPool
    {
    public:
        template <typename T>
        std::shared_ptr<T> GetResource(const typename ResourceTraits<T>::Desc& description)
        {
            if constexpr(std::is_same_v<T, Buffer>)
                return m_Buffers.GetResource(description);
            else if constexpr(std::is_same_v<T, Texture>)
                return m_Textures.GetResource(description);
            else
                static_assert(!sizeof(T), "No match for type");
            std::unreachable();
        }
        template <typename T>
        std::shared_ptr<T> AddExternalResource(const T& resource)
        {
            if constexpr(std::is_same_v<T, Buffer>)
            {
                m_Buffers.Resources.push_back(std::make_shared<T>(resource));
                return m_Buffers.Resources.back();
            }
            else if constexpr(std::is_same_v<T, Texture>)
            {
                m_Textures.Resources.push_back(std::make_shared<T>(resource));
                return m_Textures.Resources.back();
            }
            else
            {
                static_assert(!sizeof(T), "No match for type");
            }
            std::unreachable();   
        }
    private:
        template <typename T>
        struct Pool
        {
            std::shared_ptr<T> GetResource(const typename ResourceTraits<T>::Desc& description)
            {
                for (auto& resource : Resources)
                    if (resource.use_count() == 1 && ResourceAliasTraits<T>::CanAlias(
                        description, resource->GetDescription()))
                        return resource;

                // allocate new resource
                Resources.push_back(std::make_shared<T>(T::Builder(description).Build()));
                
                return Resources.back();
            }
            std::vector<std::shared_ptr<T>> Resources;
        };

        Pool<Buffer> m_Buffers;
        Pool<Texture> m_Textures;
    };

    class Graph
    {
        friend class Resources;
    public:
        Graph();
        ~Graph();
        
        template <typename PassData, typename SetupFn, typename CallbackFn>
        Pass& AddRenderPass(const std::string& name, SetupFn&& setup, CallbackFn&& callback);

        Resource AddExternal(const std::string& name, const Buffer& buffer);
        Resource AddExternal(const std::string& name, const Texture& texture);
        Resource CreateResource(const std::string& name, const GraphBufferDescription& description);
        Resource CreateResource(const std::string& name, const GraphTextureDescription& description);
        Resource Read(Resource resource, ResourceAccessFlags readFlags);
        Resource Write(Resource resource, ResourceAccessFlags writeFlags);
        Resource RenderTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore);
        Resource RenderTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore,
            const glm::vec4& clearColor);
        Resource DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
            AttachmentStore onStore);
        Resource DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
                AttachmentStore onStore, f32 clearDepth, u32 clearStencil = 0);

        DeletionQueue& GetFrameDeletionQueue() { return m_DeletionQueue; }
        DescriptorAllocator& GetFrameDescriptorAllocator() { return m_DescriptorAllocator; }
        DescriptorArenaAllocator& GetDescriptorResourceAllocator() const { return *m_DescriptorResourceArenaAllocator; }
        DescriptorArenaAllocator& GetDescriptorSamplerAllocator() const { return *m_DescriptorSamplerArenaAllocator; }

        void Clear();
        void Compile();
        void Execute(FrameContext& frameContext);
        std::string MermaidDump() const;
    private:
        Resource CreateResource(const std::string& name, const BufferDescription& description);
        Resource CreateResource(const std::string& name, const TextureDescription& description);
        
        std::pair<PipelineStage, PipelineAccess> InferResourceReadAccess(BufferDescription& description,
            ResourceAccessFlags readFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceReadAccess(TextureDescription& description,
            ResourceAccessFlags readFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceWriteAccess(BufferDescription& description,
            ResourceAccessFlags writeFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceWriteAccess(TextureDescription& description,
            ResourceAccessFlags writeFlags);

        ResourceTypeBase& GetResourceTypeBase(Resource resource) const;
        Resource AddOrCreateAccess(Resource resource, PipelineStage stage, PipelineAccess access);
        Resource AddAccess(ResourceAccess& resource, PipelineStage stage, PipelineAccess access);
    private:

        template <typename T>
        struct ExternalResource
        {
            T* Resource{nullptr};
        };
        using ExternalTexture = ExternalResource<Texture>;
        using ExternalBuffer = ExternalResource<Buffer>;
        
        std::vector<GraphBuffer> m_Buffers;
        std::vector<GraphTexture> m_Textures;
        std::vector<std::unique_ptr<Pass>> m_RenderPasses;

        Pass* m_ResourceTarget{nullptr};

        RenderGraphPool m_Pool;
        DeletionQueue m_DeletionQueue;
        DescriptorAllocator m_DescriptorAllocator;
        std::unique_ptr<DescriptorArenaAllocator> m_DescriptorResourceArenaAllocator;
        std::unique_ptr<DescriptorArenaAllocator> m_DescriptorSamplerArenaAllocator;
    };

    class Resources
    {
    public:
        Resources(Graph& graph)
            : m_Graph(&graph) {}

        const Buffer& GetBuffer(Resource resource) const;
        template <typename T>
        const Buffer& GetBuffer(Resource resource, const T& data, ResourceUploader& resourceUploader) const;
        template <typename T>
        const Buffer& GetBuffer(Resource resource, const T& data, u64 offset, ResourceUploader& resourceUploader) const;
        const Buffer& GetBuffer(Resource resource, const void* data, u64 sizeBytes,
            ResourceUploader& resourceUploader) const;
        const Buffer& GetBuffer(Resource resource, const void* data, u64 sizeBytes, u64 offset,
            ResourceUploader& resourceUploader) const;
        const Texture& GetTexture(Resource resource) const;
        Texture& GetTexture(Resource resource);
        const Graph* GetGraph() const { return m_Graph; }
    private:
        Graph* m_Graph{nullptr};
    };

    template <typename PassData, typename SetupFn, typename CallbackFn>
    Pass& Graph::AddRenderPass(const std::string& name, SetupFn&& setup, CallbackFn&& callback)
    {
        m_RenderPasses.push_back(std::make_unique<Pass>(name));
        Pass* pass = m_RenderPasses.back().get();
        
        m_ResourceTarget = pass;
        
        PassData passData = {};
        setup(*this, passData);
        
        pass->m_ExecutionCallback = std::make_unique<Pass::ExecutionCallback<PassData, CallbackFn>>(
            passData, std::forward<CallbackFn>(callback));

        m_ResourceTarget = nullptr;
        
        return *pass;
    }

    template <typename T>
    const Buffer& Resources::GetBuffer(Resource resource, const T& data, ResourceUploader& resourceUploader) const
    {
        return GetBuffer(resource, data, 0, resourceUploader);
    }

    template <typename T>
    const Buffer& Resources::GetBuffer(Resource resource, const T& data, u64 offset,
        ResourceUploader& resourceUploader) const
    {
        return GetBuffer(resource, (void*)&data, sizeof(T), offset, resourceUploader);
    }
}



