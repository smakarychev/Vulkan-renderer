#pragma once

#include "Rendering/Buffer.h"
#include "Rendering/Image/Image.h"

namespace RenderGraph
{
    class Resource
    {
        friend class Graph;
        friend class Pass;
        friend class Resources;
        friend std::ostream & operator<<(std::ostream &os, Resource renderGraphResource);
        auto operator<=>(const Resource& other) const = default;

        static constexpr u32 TYPE_BITS_COUNT = 1u;
        static constexpr u32 INDEX_BITS_COUNT = 32u - TYPE_BITS_COUNT;
        static constexpr u32 TYPE_BITS_MASK = (1u << TYPE_BITS_COUNT) - 1;
        static constexpr u32 INDEX_BITS_MASK = (1u << INDEX_BITS_COUNT) - 1;
        static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();

        static constexpr u32 BUFFER_TYPE = 0;
        static constexpr u32 TEXTURE_TYPE = 1;
    public:
        constexpr bool IsValid() const { return m_Value != NON_INDEX; }
    private:
        constexpr bool IsBuffer() const { return ((m_Value >> INDEX_BITS_COUNT) & TYPE_BITS_MASK) == BUFFER_TYPE; }
        constexpr bool IsTexture() const { return ((m_Value >> INDEX_BITS_COUNT) & TYPE_BITS_MASK) == TEXTURE_TYPE; }
        constexpr u32 Index() const { return m_Value & INDEX_BITS_MASK; }

        static constexpr Resource Texture(u32 index)
        {
            Resource texture;
            texture.m_Value = (TEXTURE_TYPE << INDEX_BITS_COUNT) | index;
            
            return texture;
        }
        static constexpr Resource Buffer(u32 index)
        {
            Resource buffer;
            buffer.m_Value = (BUFFER_TYPE << INDEX_BITS_COUNT) | index;
            
            return buffer;
        }
    private:
        u32 m_Value{NON_INDEX};
    };

    inline std::ostream& operator<<(std::ostream& os, Resource renderGraphResource)
    {
        if (renderGraphResource.m_Value == Resource::NON_INDEX)
            os << "Resource(EMPTY)";
        else if (renderGraphResource.IsBuffer())
            os << std::format("Resource(BUFFER: {})", renderGraphResource.Index());
        else
            os << std::format("Resource(TEXTURE: {})", renderGraphResource.Index());

        return os;
    }

    class ResourceAccess
    {
        friend class Graph;
        friend class Pass;
    private:
        static constexpr bool HasWriteAccess(PipelineAccess access)
        {
            return enumHasAny(access,
                PipelineAccess::WriteShader | PipelineAccess::WriteAll |
                PipelineAccess::WriteColorAttachment | PipelineAccess::WriteDepthStencilAttachment |
                PipelineAccess::WriteTransfer);
        }
        static constexpr bool HasReadAccess(PipelineAccess access)
        {
            return enumHasAny(access,
                PipelineAccess::ReadAll | PipelineAccess::ReadAttribute |
                PipelineAccess::ReadConditional | PipelineAccess::ReadHost |
                PipelineAccess::ReadIndex | PipelineAccess::ReadIndirect |
                PipelineAccess::ReadSampled | PipelineAccess::ReadShader |
                PipelineAccess::ReadStorage | PipelineAccess::ReadUniform |
                PipelineAccess::ReadDepthStencilAttachment | PipelineAccess::ReadColorAttachment |
                PipelineAccess::ReadFeedbackCounter | PipelineAccess::ReadInputAttachment |
                PipelineAccess::ReadTransfer);
        }
    private:
        Resource m_Resource{};
        PipelineStage m_Stage{PipelineStage::None};
        PipelineAccess m_Access{PipelineAccess::None};
    };

    class RenderTargetAccess
    {
        friend class Graph;
        friend class Pass;
    private:
        Resource m_Resource{};
        AttachmentLoad m_OnLoad{AttachmentLoad::Unspecified};
        AttachmentStore m_OnStore{AttachmentStore::Unspecified};
        glm::vec4 m_ClearColor{};
    };

    class DepthStencilAccess
    {
        friend class Graph;
        friend class Pass;
    private:
        Resource m_Resource{};
        AttachmentLoad m_OnLoad{AttachmentLoad::Unspecified};
        AttachmentStore m_OnStore{AttachmentStore::Unspecified};
        f32 m_ClearDepth{};
        u32 m_ClearStencil{};
        bool m_IsDepthOnly{false};
    };

    template <typename T>
    struct ResourceTraits {};

    template <>
    struct ResourceTraits<Buffer>
    {
        using Desc = BufferDescription;
    };

    template <>
    struct ResourceTraits<Image>
    {
        using Desc = ImageDescription;
    };

    class ResourceTypeBase
    {
        friend class Graph;
    public:
        ResourceTypeBase(std::string name)
            : m_Name(std::move(name)) {}
    protected:
        std::string m_Name{};
        Resource m_Rename{};

        static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
        u32 m_FirstAccess{NON_INDEX};
        u32 m_LastAccess{NON_INDEX};
        bool m_IsExternal{false};
    };

    template <typename T>
    class ResourceType : public ResourceTypeBase
    {
        friend class Graph;
        friend class Resources;
        
        using Desc = typename ResourceTraits<T>::Desc;
        using Type = T;
    public:
        ResourceType(const std::string& name, const Desc& desc)
            : ResourceTypeBase(name), m_Description(desc) {}

        void SetPhysicalResource(std::shared_ptr<T> resource)
        {
            m_ResourceRef = resource;
            m_Resource = resource.get();
        }
        void ReleaseResource()
        {
            ASSERT(m_Resource, "Cannot release an empty result")
            m_ResourceRef = nullptr;
        }
    private:
        // store both ref count and raw pointer to be able to alias resources
        std::shared_ptr<T> m_ResourceRef{};
        T* m_Resource{};
        Desc m_Description{};
    };

    using GraphBuffer = ResourceType<Buffer>;
    using GraphTexture = ResourceType<Image>;

    struct GraphBufferDescription
    {
        u64 SizeBytes;
    };

    struct GraphTextureDescription
    {
        u32 Width;
        u32 Height;
        u32 Layers{1};
        u32 Mipmaps{1};
        Format Format;
        ImageKind Kind{ImageKind::Image2d};
        ImageFilter MipmapFilter{ImageFilter::Linear};
        std::vector<ImageSubresourceDescription::Packed> AdditionalViews;
    };
}

