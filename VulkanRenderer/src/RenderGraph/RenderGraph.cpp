#include "RenderGraph.h"

#include <sstream>
#include <unordered_set>

#include "Renderer.h"
#include "Rendering/Synchronization.h"
#include "Vulkan/RenderCommand.h"

namespace RenderGraph
{
    Graph::Graph()
    {
        m_DescriptorAllocator = DescriptorAllocator::Builder()
            .SetMaxSetsPerPool(10000)
            .Build();

        m_DescriptorResourceArenaAllocator = std::make_unique<DescriptorArenaAllocator>(
            DescriptorArenaAllocator::Builder()
                .Kind(DescriptorAllocatorKind::Resources)
                .Residence(DescriptorAllocatorResidence::CPU)
                .ForTypes({DescriptorType::UniformBuffer, DescriptorType::StorageBuffer})
                .Count(1000)
                .Build());
        
        m_DescriptorSamplerArenaAllocator = std::make_unique<DescriptorArenaAllocator>(
            DescriptorArenaAllocator::Builder()
                .Kind(DescriptorAllocatorKind::Samplers)
                .Residence(DescriptorAllocatorResidence::CPU)
                .ForTypes({DescriptorType::Sampler})
                .Count(32)
                .Build());
    }

    Graph::~Graph()
    {
        m_DeletionQueue.Flush();
    }

    Resource Graph::AddExternal(const std::string& name, const Buffer& buffer)
    {
        Resource bufferResource = CreateResource(name, buffer.GetDescription());
        GetResourceTypeBase(bufferResource).m_IsExternal = true;
        m_Buffers[bufferResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(buffer));

        return bufferResource;
    }

    Resource Graph::AddExternal(const std::string& name, const Texture& texture)
    {
        Resource textureResource = CreateResource(name, texture.GetDescription());
        GetResourceTypeBase(textureResource).m_IsExternal = true;
        m_Textures[textureResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(texture));

        return textureResource;
    }

    Resource Graph::CreateResource(const std::string& name, const GraphBufferDescription& description)
    {
        return CreateResource(name, BufferDescription{
            .SizeBytes = description.SizeBytes,
            .Usage = BufferUsage::None});
    }

    Resource Graph::CreateResource(const std::string& name, const GraphTextureDescription& description)
    {
        return CreateResource(name, TextureDescription{
            .Width = description.Width,
            .Height = description.Height,
            .Layers = description.Layers,
            .Mipmaps = description.Mipmaps,
            .Views = description.Views,
            .Format = description.Format,
            .Kind = description.Kind,
            .Usage = ImageUsage::None,
            .MipmapFilter = description.MipmapFilter});
    }

    Resource Graph::CreateResource(const std::string& name, const BufferDescription& description)
    {
        Resource resource = Resource::Buffer((u32)m_Buffers.size());
        m_Buffers.emplace_back(name, description);

        return resource;
    }

    Resource Graph::CreateResource(const std::string& name, const TextureDescription& description)
    {
        Resource resource = Resource::Texture((u32)m_Textures.size());
        m_Textures.emplace_back(name, description);

        return resource;
    }

    Resource Graph::Read(Resource resource, ResourceAccessFlags readFlags)
    {
        ASSERT(m_ResourceTarget, "Call to 'Read' outside of 'SetupFn' of render pass")

        std::pair<PipelineStage, PipelineAccess> stageAccess = {};
        if (resource.IsBuffer())
            stageAccess = InferResourceReadAccess(m_Buffers[resource.Index()].m_Description, readFlags);
        else
            stageAccess = InferResourceReadAccess(m_Textures[resource.Index()].m_Description, readFlags);
        auto&& [stage, access] = stageAccess;

        return AddOrCreateAccess(resource, stage, access);
    }

    Resource Graph::Write(Resource resource, ResourceAccessFlags writeFlags)
    {
        ASSERT(m_ResourceTarget, "Call to 'Write' outside of 'SetupFn' of render pass")

        std::pair<PipelineStage, PipelineAccess> stageAccess = {};
        if (resource.IsBuffer())
            stageAccess = InferResourceWriteAccess(m_Buffers[resource.Index()].m_Description, writeFlags);
        else
            stageAccess = InferResourceWriteAccess(m_Textures[resource.Index()].m_Description, writeFlags);
        auto&& [stage, access] = stageAccess;

        return AddOrCreateAccess(resource, stage, access);
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return RenderTarget(resource, onLoad, onStore, {});
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, const glm::vec4& clearColor)
    {
        ASSERT(m_ResourceTarget, "Call to 'RenderTarget' outside of 'SetupFn' of render pass")

        RenderTargetAccess renderTargetAccess = {};

        renderTargetAccess.m_Resource = AddOrCreateAccess(resource,
            PipelineStage::ColorOutput, PipelineAccess::WriteColorAttachment);
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Color;
        renderTargetAccess.m_ClearColor = clearColor;
        renderTargetAccess.m_OnLoad = onLoad;
        renderTargetAccess.m_OnStore = onStore;

        m_ResourceTarget->m_RenderTargetAttachmentAccess.push_back(renderTargetAccess);

        return renderTargetAccess.m_Resource;
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return DepthStencilTarget(resource, onLoad, onStore, 0.0, 0);
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, f32 clearDepth, u32 clearStencil)
    {
        ASSERT(m_ResourceTarget, "Call to 'DepthStencilTarget' outside of 'SetupFn' of render pass")

        DepthStencilAccess depthStencilAccess = {};
        
        depthStencilAccess.m_Resource = AddOrCreateAccess(resource,
            PipelineStage::DepthLate, PipelineAccess::WriteDepthStencilAttachment);
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Depth | ImageUsage::Stencil;
        depthStencilAccess.m_ClearDepth = clearDepth;
        depthStencilAccess.m_ClearStencil = clearStencil;
        depthStencilAccess.m_OnLoad = onLoad;
        depthStencilAccess.m_OnStore = onStore;
        depthStencilAccess.m_IsDepthOnly = m_Textures[resource.Index()].m_Description.Format == Format::D32_FLOAT;

        m_ResourceTarget->m_DepthStencilAccess = depthStencilAccess;

        return depthStencilAccess.m_Resource;
    }

    void Graph::Clear()
    {
        m_DeletionQueue.Flush();
        m_DescriptorAllocator.ResetPools();
        for (auto& pass: m_RenderPasses)
        {
            pass->m_LayoutTransitions.clear();
            pass->m_Barriers.clear();

            pass->m_LayoutTransitionInfos.clear();
            pass->m_Barriers.clear();

            for (auto& resourceAccess : pass->m_Accesses)
            {
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);
                resourceTypeBase.m_FirstAccess = resourceTypeBase.m_LastAccess = ResourceTypeBase::NON_INDEX;
            }
        }
    }

    // todo: this is a mess. fix me
    void Graph::Compile()
    {
        Clear();

        // todo: this function iterates over passes and accesses an unnecessary amount of times,
        // it probably is completely irrelevant and will be optimized, but I probably should change it still
        
        // determine resources usage spans, assuming the passes are ordered correctly
        // todo: pass reordering
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            bool isRasterizationPass = true;
            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                isRasterizationPass = isRasterizationPass && !enumHasAny(
                    resourceAccess.m_Stage, PipelineStage::ComputeShader);
                
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);
                resourceTypeBase.m_FirstAccess = resourceTypeBase.m_FirstAccess == ResourceTypeBase::NON_INDEX ?
                    passIndex : resourceTypeBase.m_FirstAccess;
                resourceTypeBase.m_LastAccess = passIndex;

                ASSERT(resourceAccess.m_Stage != PipelineStage::None,
                    "Stage is not inferrable for {}", resourceTypeBase.m_Name)
                ASSERT(resourceAccess.m_Access != PipelineAccess::None,
                    "Access is not inferrable for {}", resourceTypeBase.m_Name)
                ASSERT(resourceAccess.m_Resource.IsBuffer() ?
                    m_Buffers[resourceAccess.m_Resource.Index()].m_Description.Usage != BufferUsage::None :
                    m_Textures[resourceAccess.m_Resource.Index()].m_Description.Usage != ImageUsage::None,
                    "Usage is not inferrable for {}", resourceTypeBase.m_Name)
            }

            Pass& pass = *m_RenderPasses[passIndex];
            pass.m_IsRasterizationPass = isRasterizationPass &&
                !(pass.m_RenderTargetAttachmentAccess.empty() && !pass.m_DepthStencilAccess.has_value());
        }

        // allocate resources
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);

                if (resourceTypeBase.m_IsExternal)
                    continue;
                
                if (resourceTypeBase.m_FirstAccess == passIndex)
                {
                    if (resourceAccess.m_Resource.IsBuffer())
                    {
                        GraphBuffer& buffer = m_Buffers[resourceAccess.m_Resource.Index()];
                        // all buffers require device address
                        buffer.m_Description.Usage |= BufferUsage::DeviceAddress;
                        // todo: definitely not the best way
                        buffer.m_Description.Usage |= BufferUsage::Destination;
                        buffer.SetPhysicalResource(m_Pool.GetResource<Buffer>(buffer.m_Description));
                    }
                    else
                    {
                        GraphTexture& texture = m_Textures[resourceAccess.m_Resource.Index()];
                        texture.SetPhysicalResource(m_Pool.GetResource<Texture>(texture.m_Description));   
                    }
                }
                if (resourceTypeBase.m_LastAccess == passIndex)
                {
                    if (resourceAccess.m_Resource.IsBuffer())
                        m_Buffers[resourceAccess.m_Resource.Index()].ReleaseResource();
                    else
                        m_Textures[resourceAccess.m_Resource.Index()].ReleaseResource();
                }
            }
        }

        // inject barriers
        struct LayoutInfo
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageLayout Layout{ImageLayout::Undefined};
        };
        std::vector currentLayouts(m_Textures.size(), LayoutInfo{});
        enum class AccessType
        {
            None, Read, Write
        };
        struct AccessInfo
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            AccessType Type{AccessType::None};
        };
        std::vector currentBufferAccess(m_Buffers.size(), AccessInfo{});
        std::vector currentTextureAccess(m_Textures.size(), AccessInfo{});
        
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = m_RenderPasses[passIndex];

            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                AccessType accessType = ResourceAccess::HasWriteAccess(resourceAccess.m_Access) ?
                    AccessType::Write : AccessType::Read;
                Resource resource = resourceAccess.m_Resource;
                if (GetResourceTypeBase(resource).m_FirstAccess == passIndex)
                {
                    AccessInfo accessInfo = {
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType};
                    if (resource.IsTexture())
                    {
                        currentTextureAccess[resource.Index()] = accessInfo;
                    }
                    else
                    {
                        currentBufferAccess[resource.Index()] = accessInfo;
                        continue;
                    }
                }
                        
                auto& texture = m_Textures[resource.Index()];

                // layout transitions
                auto inferDesiredLayout = [this, &texture](const ResourceAccess& access) -> ImageLayout
                {
                    if (ResourceAccess::HasWriteAccess(access.m_Access))
                    {
                        if (enumHasAny(access.m_Stage, PipelineStage::ComputeShader))
                            return ImageLayout::General;
                        if (enumHasAny(access.m_Access, PipelineAccess::WriteColorAttachment))
                            return ImageLayout::ColorAttachment;
                        if (enumHasAny(access.m_Access, PipelineAccess::WriteDepthStencilAttachment))
                            return texture.m_Description.Format == Format::D32_FLOAT ?
                                ImageLayout::DepthAttachment : ImageLayout::DepthStencilAttachment;
                        return ImageLayout::Attachment;
                    }

                    if (enumHasAny(access.m_Access, PipelineAccess::ReadDepthStencilAttachment))
                        return texture.m_Description.Format == Format::D32_FLOAT ?
                            ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly;
                    return ImageLayout::ReadOnly;
                };
                
                ImageLayout currentLayout = currentLayouts[resource.Index()].Layout;
                ImageLayout desiredLayout = inferDesiredLayout(resourceAccess);

                if (currentLayout != desiredLayout)
                {
                    ImageSubresource subresource = texture.m_Resource->CreateSubresource();
                    LayoutTransitionInfo layoutTransitionInfo = {
                        .ImageSubresource = &subresource,
                        .SourceStage = currentLayouts[resource.Index()].Stage,
                        .DestinationStage = resourceAccess.m_Stage,
                        .SourceAccess = currentLayouts[resource.Index()].Access,
                        .DestinationAccess = resourceAccess.m_Access,
                        .OldLayout = currentLayout,
                        .NewLayout = desiredLayout};
                    
                    pass->m_LayoutTransitions.push_back(
                        DependencyInfo::Builder().LayoutTransition(layoutTransitionInfo).Build(m_DeletionQueue));
                    pass->m_LayoutTransitionInfos.push_back(Pass::PassTextureTransitionInfo{
                        .Texture = resource,
                        .OldLayout = currentLayout,
                        .NewLayout = desiredLayout});
                    
                    currentLayouts[resource.Index()] = {
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Layout = desiredLayout};

                    continue;
                }


                if (GetResourceTypeBase(resource).m_FirstAccess == passIndex)
                    continue;
                
                AccessInfo currentAccessInfo = resource.IsBuffer() ?
                    currentBufferAccess[resource.Index()] : currentTextureAccess[resource.Index()];
                
                if (currentAccessInfo.Type == AccessType::Read && accessType == AccessType::Write)
                {
                    // simple execution barrier
                    ExecutionDependencyInfo executionDependencyInfo = {
                        .SourceStage = currentAccessInfo.Stage,
                        .DestinationStage = resourceAccess.m_Stage};
                    pass->m_Barriers.push_back(
                        DependencyInfo::Builder().ExecutionDependency(executionDependencyInfo).Build(m_DeletionQueue));
                    pass->m_BarrierDependencyInfos.push_back({
                        .Resource = resource,
                        .ExecutionDependency = executionDependencyInfo});
                }
                else if (!(currentAccessInfo.Type == AccessType::Read && accessType == AccessType::Read))
                {
                    // memory barrier
                    MemoryDependencyInfo memoryDependencyInfo = {
                        .SourceStage = currentAccessInfo.Stage,
                        .DestinationStage = resourceAccess.m_Stage,
                        .SourceAccess = currentAccessInfo.Access,
                        .DestinationAccess = resourceAccess.m_Access};
                    pass->m_Barriers.push_back(
                        DependencyInfo::Builder().MemoryDependency(memoryDependencyInfo).Build(m_DeletionQueue));
                    pass->m_BarrierDependencyInfos.push_back({
                        .Resource = resource,
                        .MemoryDependency = memoryDependencyInfo});
                }
            }
        }
    }

    void Graph::Execute(FrameContext& frameContext)
    {
        Resources resources = {*this};
        for (auto& pass : m_RenderPasses)
        {
            for (auto& barrier : pass->m_Barriers)
                RenderCommand::WaitOnBarrier(frameContext.Cmd, barrier);
            for (auto& layoutTransition : pass->m_LayoutTransitions)
                RenderCommand::WaitOnBarrier(frameContext.Cmd, layoutTransition);

            if (pass->m_IsRasterizationPass)
            {
                glm::uvec2 resolution = pass->m_RenderTargetAttachmentAccess.empty() ?
                    glm::uvec2{
                        m_Textures[pass->m_DepthStencilAccess->m_Resource.Index()].m_Description.Width,
                        m_Textures[pass->m_DepthStencilAccess->m_Resource.Index()].m_Description.Height
                    } :
                    glm::uvec2{
                        m_Textures[pass->m_RenderTargetAttachmentAccess.front().m_Resource.Index()].m_Description.Width,
                        m_Textures[pass->m_RenderTargetAttachmentAccess.front().m_Resource.Index()].m_Description.Height
                    };
                RenderCommand::SetViewport(frameContext.Cmd, resolution);
                RenderCommand::SetScissors(frameContext.Cmd, {0, 0}, resolution);

                std::vector<RenderingAttachment> attachments;
                attachments.reserve(pass->m_RenderTargetAttachmentAccess.size() +
                    (u32)pass->m_DepthStencilAccess.has_value());
                for (auto& target : pass->m_RenderTargetAttachmentAccess)
                    attachments.push_back(RenderingAttachment::Builder()
                        .ClearValue(target.m_ClearColor)
                        .SetType(RenderingAttachmentType::Color)
                        .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                        .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource, ImageLayout::ColorAttachment)
                        .Build(m_DeletionQueue));
                if (pass->m_DepthStencilAccess.has_value())
                {
                    auto target = pass->m_DepthStencilAccess.value();

                    ImageLayout layout = target.m_IsDepthOnly ?
                        ImageLayout::DepthAttachment : ImageLayout::DepthStencilAttachment;
                    
                    attachments.push_back(RenderingAttachment::Builder()
                       .ClearValue(target.m_ClearDepth, target.m_ClearStencil)
                       .SetType(RenderingAttachmentType::Depth)
                       .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                       .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource, layout)
                       .Build(m_DeletionQueue));
                }
                RenderingInfo::Builder renderingInfoBuilder = RenderingInfo::Builder()
                    .SetRenderArea(resolution);
                for (auto& attachment : attachments)
                    renderingInfoBuilder.AddAttachment(attachment);

                RenderingInfo renderingInfo = renderingInfoBuilder.Build(m_DeletionQueue);
                
                RenderCommand::BeginRendering(frameContext.Cmd, renderingInfo);

                pass->Execute(frameContext, resources);

                RenderCommand::EndRendering(frameContext.Cmd);
            }
            else
            {
                pass->Execute(frameContext, resources);
            }
            
        }
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceReadAccess(BufferDescription& description,
        ResourceAccessFlags readFlags)
    {
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            BufferUsage Usage{BufferUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Index,
                ResourceSubAccess{
                    .Stage = PipelineStage::IndexInput,
                    .Access = PipelineAccess::ReadIndex,
                    .Usage = BufferUsage::Index}
            },
            {
                ResourceAccessFlags::Indirect,
                ResourceSubAccess{
                    .Stage = PipelineStage::Indirect,
                    .Access = PipelineAccess::ReadIndirect,
                    .Usage = BufferUsage::Indirect}
            },
            {
                ResourceAccessFlags::Conditional,
                ResourceSubAccess{
                    .Stage = PipelineStage::ConditionalRendering,
                    .Access = PipelineAccess::ReadConditional,
                    .Usage = BufferUsage::Conditional}
            },
            {
                ResourceAccessFlags::Attribute,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexInput,
                    .Access = PipelineAccess::ReadAttribute,
                    .Usage = BufferUsage::Vertex}
            },
            {
                ResourceAccessFlags::Uniform,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadUniform,
                    .Usage = BufferUsage::Uniform}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadStorage,
                    .Usage = BufferUsage::Storage}
            },
        }; 
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;

        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(readFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }
        
        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceReadAccess(TextureDescription& description,
        ResourceAccessFlags readFlags)
    {
        ASSERT(!enumHasAny(readFlags,
            ResourceAccessFlags::Attribute |
            ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform), "Image read has inappropriate flags")
        ASSERT(
            enumHasAny(readFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
            enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
            "Image read does not have appropriate flags")
        
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageUsage Usage{ImageUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadStorage,
                    .Usage = ImageUsage::Storage}
            },
            {
                ResourceAccessFlags::Sampled,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadSampled,
                    .Usage = ImageUsage::Sampled}
            },
        };
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;

        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(readFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }

        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceWriteAccess(BufferDescription& description,
        ResourceAccessFlags writeFlags)
    {
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            BufferUsage Usage{BufferUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Index,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Index}
            },
            {
                ResourceAccessFlags::Indirect,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Indirect}
            },
            {
                ResourceAccessFlags::Conditional,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Conditional}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Storage}
            },
        }; 
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;
        
        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(writeFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }
        
        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceWriteAccess(TextureDescription& description,
        ResourceAccessFlags writeFlags)
    {
        ASSERT(!enumHasAny(writeFlags,
           ResourceAccessFlags::Attribute |
           ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
           ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform), "Image read has inappropriate flags")
       ASSERT(
           enumHasAny(writeFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
           enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
           "Image read does not have appropriate flags")
        
       struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageUsage Usage{ImageUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = ImageUsage::Storage}
            },
            {
                ResourceAccessFlags::Sampled,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = ImageUsage::Sampled}
            },
        };
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;
        
        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(writeFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }

        return std::make_pair(stage, access);
    }

    ResourceTypeBase& Graph::GetResourceTypeBase(Resource resource) const
    {
        if (resource.IsTexture())
        {
            const GraphTexture& texture = m_Textures[resource.Index()];
            return (ResourceTypeBase&)texture;
        }
        
        const GraphBuffer& buffer = m_Buffers[resource.Index()];
        return (ResourceTypeBase&)buffer;
    }

    Resource Graph::AddOrCreateAccess(Resource resource, PipelineStage stage,
        PipelineAccess access)
    {
        auto it = std::ranges::find_if(m_ResourceTarget->m_Accesses,
            [resource](auto& resourceAccess) { return resourceAccess.m_Resource == resource; });

        if (it != m_ResourceTarget->m_Accesses.end())
            return AddAccess(*it, stage, access);

        ResourceAccess GraphResourceAccess = {};
        GraphResourceAccess.m_Resource = resource;
        GraphResourceAccess.m_Stage = stage;
        GraphResourceAccess.m_Access = access;
        m_ResourceTarget->m_Accesses.push_back(GraphResourceAccess);
        
        return resource;
    }

    Resource Graph::AddAccess(ResourceAccess& resource, PipelineStage stage,
        PipelineAccess access)
    {
        ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resource.m_Resource);
        ASSERT(
            !ResourceAccess::HasWriteAccess(access) ||
            !ResourceAccess::HasWriteAccess(resource.m_Access) ||
            resourceTypeBase.m_Rename.m_Value == Resource::NON_INDEX, "Cannot write twice to the same resource")

        resource.m_Stage |= stage;
        resource.m_Access |= access;
        
        if (ResourceAccess::HasWriteAccess(access))
        {
            resourceTypeBase.m_Rename = resource.m_Resource.IsBuffer() ?
                CreateResource(resourceTypeBase.m_Name,
                    m_Buffers[resource.m_Resource.Index()].m_Description) :
                CreateResource(resourceTypeBase.m_Name,
                    m_Textures[resource.m_Resource.Index()].m_Description);

            return resourceTypeBase.m_Rename;
        }

        return resource.m_Resource;
    }

    const Buffer& Resources::GetBuffer(Resource resource) const
    {
        ASSERT(resource.IsBuffer(), "Provided resource handle is not a buffer")

        return *m_Graph->m_Buffers[resource.Index()].m_Resource;
    }

    const Buffer& Resources::GetBuffer(Resource resource, const void* data, u64 sizeBytes,
        ResourceUploader& resourceUploader) const
    {
        return GetBuffer(resource, data, sizeBytes, 0, resourceUploader);
    }

    const Buffer& Resources::GetBuffer(Resource resource, const void* data, u64 sizeBytes, u64 offset,
        ResourceUploader& resourceUploader) const
    {
        ASSERT(resource.IsBuffer(), "Provided resource handle is not a buffer")

        Buffer& buffer = const_cast<Buffer&>(GetBuffer(resource));
        resourceUploader.UpdateBuffer(buffer, data, sizeBytes, offset);

        return buffer;
    }

    const Texture& Resources::GetTexture(Resource resource) const
    {
        ASSERT(resource.IsTexture(), "Provided resource handle is not a texture")

        return *m_Graph->m_Textures[resource.Index()].m_Resource;
    }

    Texture& Resources::GetTexture(Resource resource)
    {
        return const_cast<Texture&>(const_cast<const Resources&>(*this).GetTexture(resource));
    }

    std::string Graph::MermaidDump() const
    {
        auto getPassId = [](u32 passIndex)
        {
            return std::format("pass_{}", passIndex);
        };
        auto getTransitionId = [](u32 passIndex, u32 transitionIndex)
        {
            return std::format("transition_{}_{}", passIndex, transitionIndex);
        };
        auto getBarrierId = [](u32 passIndex, u32 barrierIndex)
        {
            return std::format("barrier_{}_{}", passIndex, barrierIndex);
        };
        
        std::stringstream ss;
        ss << std::format("graph LR\n");

        // declare-nodes
        std::unordered_set<u32> declaredAccesses = {};
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = *m_RenderPasses[passIndex];
            std::string passName = std::format("\t{}[/Pass_{}/]", getPassId(passIndex), pass.m_Name);
            ss << std::format("{}\n", passName);

            for (auto& access : pass.m_Accesses)
            {
                if (declaredAccesses.contains(access.m_Resource.m_Value))
                    continue;
                
                std::string resourceName = GetResourceTypeBase(access.m_Resource).m_Name;
                ss << std::format("\t{}[\"`{}\n", access.m_Resource.m_Value, resourceName);
                if (access.m_Resource.IsBuffer())
                {
                    const BufferDescription& description = m_Buffers[access.m_Resource.Index()].m_Description;
                    ss << std::format("\t{}\n\t{}`\"]\n", description.SizeBytes,
                        BufferUtils::bufferUsageToString(description.Usage));
                }
                else
                {
                    const TextureDescription& description = m_Textures[access.m_Resource.Index()].m_Description;
                    ss << std::format("\t({} x {} x {})\n\t{}\n\t{}\n\t{}\n\t{}`\"]\n",
                        description.Width, description.Height, description.Layers,
                        ImageUtils::imageKindToString(description.Kind),
                        FormatUtils::formatToString(description.Format),
                        ImageUtils::imageUsageToString(description.Usage),
                        ImageUtils::imageFilterToString(description.MipmapFilter));
                }
                
                declaredAccesses.emplace(access.m_Resource.m_Value);
            }

            for (u32 transitionIndex = 0; transitionIndex < pass.m_LayoutTransitions.size(); transitionIndex++)
            {
                auto& transitionInfo = pass.m_LayoutTransitionInfos[transitionIndex];
                std::string transitionId = getTransitionId(passIndex, transitionIndex);
                ss << std::format("\t{}{{{{\"`{}\n", transitionId, "Layout transition");
                ss << std::format("\t{} - {}`\"}}}}\n",
                    ImageUtils::imageLayoutToString(transitionInfo.OldLayout),
                    ImageUtils::imageLayoutToString(transitionInfo.NewLayout));
            }

            for (u32 barrierIndex = 0; barrierIndex < pass.m_Barriers.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                std::string barrierId = getBarrierId(passIndex, barrierIndex);
                ss << std::format("\t{}{{{{\"`{}\n", barrierId, "Barrier");
                if (barrierInfo.ExecutionDependency.has_value())
                {
                    auto& execution = barrierInfo.ExecutionDependency.value();
                    ss << std::format("\t{} - {}`\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(execution.SourceStage),
                        SynchronizationUtils::pipelineStageToString(execution.DestinationStage));
                }
                else
                {
                    auto& memory = barrierInfo.MemoryDependency.value();
                    ss << std::format("\t{} - {}\n\t{} - {}`\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(memory.SourceStage),
                        SynchronizationUtils::pipelineStageToString(memory.DestinationStage),
                        SynchronizationUtils::pipelineAccessToString(memory.SourceAccess),
                        SynchronizationUtils::pipelineAccessToString(memory.DestinationAccess));
                }
            }
        }
        
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = *m_RenderPasses[passIndex];
            std::unordered_map<u32, u32> transitionedTextures = {};
            for (u32 transitionIndex = 0; transitionIndex < pass.m_LayoutTransitions.size(); transitionIndex++)
            {
                auto& transitionInfo = pass.m_LayoutTransitionInfos[transitionIndex];
                transitionedTextures.emplace(transitionInfo.Texture.m_Value, transitionIndex);
            }
            std::unordered_map<u32, u32> barriers = {};
            for (u32 barrierIndex = 0; barrierIndex < pass.m_Barriers.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                barriers.emplace(barrierInfo.Resource.m_Value, barrierIndex);
            }
            for (auto& access : pass.m_Accesses)
            {
                if (ResourceAccess::HasWriteAccess(access.m_Access))
                {
                    if (transitionedTextures.contains(access.m_Resource.m_Value))
                    {
                        u32 transitionIndex = transitionedTextures.at(access.m_Resource.m_Value);
                        std::string transitionId = getTransitionId(passIndex, transitionIndex);
                        ss << std::format("\t{} -- waits for --> {}\n",
                            getPassId(passIndex), transitionId);
                        ss << std::format("\t{} -- to write to --> {}\n",
                            transitionId, access.m_Resource.m_Value);
                    }
                    else if (barriers.contains(access.m_Resource.m_Value))
                    {
                        u32 barrierIndex = barriers.at(access.m_Resource.m_Value);
                        std::string barrierId = getBarrierId(passIndex, barrierIndex);
                        ss << std::format("\t{} -- waits for --> {}\n",
                            getPassId(passIndex), barrierId);
                        ss << std::format("\t{} -- to write to --> {}\n",
                            barrierId, access.m_Resource.m_Value);
                    }
                    else
                    {
                        ss << std::format("\t{} -- writes to --> {}\n",
                            getPassId(passIndex), access.m_Resource.m_Value);
                    }
                }
                else
                {
                    if (transitionedTextures.contains(access.m_Resource.m_Value))
                    {
                        u32 transitionIndex = transitionedTextures.at(access.m_Resource.m_Value);
                        std::string transitionId = getTransitionId(passIndex, transitionIndex);
                        ss << std::format("\t{} -- transitions --> {}\n",
                            access.m_Resource.m_Value, transitionId);
                        ss << std::format("\t{} -- to be read by --> {}\n",
                            transitionId, getPassId(passIndex));
                    }
                    else if (barriers.contains(access.m_Resource.m_Value))
                    {
                        u32 barrierIndex = barriers.at(access.m_Resource.m_Value);
                        std::string barrierId = getBarrierId(passIndex, barrierIndex);
                        ss << std::format("\t{} -- waited on --> {}\n",
                            access.m_Resource.m_Value, barrierId);
                        ss << std::format("\t{} -- to be read by --> {}\n",
                            barrierId, getPassId(passIndex));
                    }
                    else
                    {
                        ss << std::format("\t{} -- read by --> {}\n",
                            access.m_Resource.m_Value, getPassId(passIndex));
                    }
                }
            }
        }

        return ss.str();
    }
}

