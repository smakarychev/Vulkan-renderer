#include "rendererpch.h"

#include "RGGraph.h"

#include "FrameContext.h"
#include "Vulkan/Device.h"

#include "RGGraphWatcher.h"
#include "Assets/Shaders/ShaderAssetManager.h"

#define RG_CHECK_RETURN(x, ...) if (!(x)) { LUX_LOG_ERROR(__VA_ARGS__); return {}; }
#define RG_CHECK_RETURN_VOID(x, ...) if (!(x)) { LUX_LOG_ERROR(__VA_ARGS__); return; }

namespace RG
{
namespace
{
struct BufferSubaccess
{
    PipelineStage Stage{PipelineStage::None};
    PipelineAccess Access{PipelineAccess::None};
    BufferUsage Usage{BufferUsage::None};
};

struct ImageSubaccess
{
    PipelineStage Stage{PipelineStage::None};
    PipelineAccess Access{PipelineAccess::None};
    ImageUsage Usage{ImageUsage::None};
};

struct StageWithAccess
{
    PipelineStage Stage{PipelineStage::None};
    PipelineAccess Access{PipelineAccess::None};
};

struct FlagsToBufferAccess
{
    ResourceAccessFlags Flags{ResourceAccessFlags::None};
    BufferSubaccess Read{};
    BufferSubaccess Write{};
};

struct FlagsToImageAccess
{
    ResourceAccessFlags Flags{ResourceAccessFlags::None};
    ImageSubaccess Read{};
    ImageSubaccess Write{};
};

constexpr std::array BUFFER_FLAGS_TO_SUB_ACCESS_MAP = {
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Vertex,
        .Read = {
            .Stage = PipelineStage::VertexShader
        },
        .Write = {
            .Stage = PipelineStage::VertexShader
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Pixel,
        .Read = {
            .Stage = PipelineStage::PixelShader
        },
        .Write = {
            .Stage = PipelineStage::PixelShader
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Compute,
        .Read = {
            .Stage = PipelineStage::ComputeShader
        },
        .Write = {
            .Stage = PipelineStage::ComputeShader
        },
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Index,
        .Read = {
            .Stage = PipelineStage::IndexInput,
            .Access = PipelineAccess::ReadIndex,
            .Usage = BufferUsage::Index
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = BufferUsage::Index
        },
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Indirect,
        .Read = {
            .Stage = PipelineStage::Indirect,
            .Access = PipelineAccess::ReadIndirect,
            .Usage = BufferUsage::Indirect
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = BufferUsage::Indirect
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Conditional,
        .Read = {
            .Stage = PipelineStage::ConditionalRendering,
            .Access = PipelineAccess::ReadConditional,
            .Usage = BufferUsage::Conditional
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = BufferUsage::Conditional
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Attribute,
        .Read = {
            .Stage = PipelineStage::VertexInput,
            .Access = PipelineAccess::ReadAttribute,
            .Usage = BufferUsage::Vertex
        },
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Uniform,
        .Read = {
            .Access = PipelineAccess::ReadUniform,
            .Usage = BufferUsage::Uniform
        },
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Storage,
        .Read = {
            .Access = PipelineAccess::ReadStorage,
            .Usage = BufferUsage::Storage
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = BufferUsage::Storage
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Copy,
        .Read = {
            .Stage = PipelineStage::Copy,
            .Access = PipelineAccess::ReadTransfer,
            .Usage = BufferUsage::Source
        },
        .Write = {
            .Stage = PipelineStage::Copy,
            .Access = PipelineAccess::WriteTransfer,
            .Usage = BufferUsage::Destination
        }
    },
    FlagsToBufferAccess{
        .Flags = ResourceAccessFlags::Readback,
        .Read = {
            .Stage = PipelineStage::Host,
            .Access = PipelineAccess::ReadHost,
            .Usage = BufferUsage::Readback
        },
    },
};
constexpr std::array IMAGE_FLAGS_TO_SUB_ACCESS_MAP = {
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Vertex,
        .Read = {
            .Stage = PipelineStage::VertexShader
        },
        .Write = {
            .Stage = PipelineStage::VertexShader
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Pixel,
        .Read = {
            .Stage = PipelineStage::PixelShader
        },
        .Write = {
            .Stage = PipelineStage::PixelShader
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Compute,
        .Read = {
            .Stage = PipelineStage::ComputeShader
        },
        .Write = {
            .Stage = PipelineStage::ComputeShader
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Storage,
        .Read = {
            .Access = PipelineAccess::ReadStorage,
            .Usage = ImageUsage::Storage
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = ImageUsage::Storage
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Sampled,
        .Read = {
            .Access = PipelineAccess::ReadSampled,
            .Usage = ImageUsage::Sampled
        },
        .Write = {
            .Access = PipelineAccess::WriteShader,
            .Usage = ImageUsage::Sampled
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Blit,
        .Read = {
            .Stage = PipelineStage::Blit,
            .Access = PipelineAccess::ReadTransfer,
            .Usage = ImageUsage::Source
        },
        .Write = {
            .Stage = PipelineStage::Blit,
            .Access = PipelineAccess::WriteTransfer,
            .Usage = ImageUsage::Destination
        }
    },
    FlagsToImageAccess{
        .Flags = ResourceAccessFlags::Copy,
        .Read = {
            .Stage = PipelineStage::Copy,
            .Access = PipelineAccess::ReadTransfer,
            .Usage = ImageUsage::Source
        },
        .Write = {
            .Stage = PipelineStage::Copy,
            .Access = PipelineAccess::WriteTransfer,
            .Usage = ImageUsage::Destination
        }
    },
};

constexpr StageWithAccess applyReadFlags(auto& description, ResourceAccessFlags flags, const auto& map)
{
    PipelineStage stage = PipelineStage::None;
    PipelineAccess access = PipelineAccess::None;
    for (auto&& [flag, read, _] : map)
    {
        if (enumHasAny(flags, flag))
        {
            stage |= read.Stage;
            access |= read.Access;
            description.Usage |= read.Usage;
        }
    }

    return {.Stage = stage, .Access = access};
}

constexpr StageWithAccess applyWriteFlags(auto& description, ResourceAccessFlags flags, const auto& map)
{
    PipelineStage stage = PipelineStage::None;
    PipelineAccess access = PipelineAccess::None;
    for (auto&& [flag, _, write] : map)
    {
        if (enumHasAny(flags, flag))
        {
            stage |= write.Stage;
            access |= write.Access;
            description.Usage |= write.Usage;
        }
    }

    return {.Stage = stage, .Access = access};
}

constexpr StageWithAccess applyReadWriteFlags(auto& description, ResourceAccessFlags flags, const auto& map)
{
    PipelineStage stage = PipelineStage::None;
    PipelineAccess access = PipelineAccess::None;
    for (auto&& [flag, read, write] : map)
    {
        if (enumHasAny(flags, flag))
        {
            stage |= read.Stage | write.Stage;
            access |= read.Access | write.Access;
            description.Usage |= read.Usage | write.Usage;
        }
    }

    return {.Stage = stage, .Access = access};
}

constexpr StageWithAccess inferBufferReadAccess(BufferDescription& description, ResourceAccessFlags readFlags)
{
    ASSERT(!enumHasAny(readFlags,
        ResourceAccessFlags::Blit | ResourceAccessFlags::Sampled), "Buffer read has inappropriate flags")

    return applyReadFlags(description, readFlags, BUFFER_FLAGS_TO_SUB_ACCESS_MAP);
}

constexpr StageWithAccess inferBufferWriteAccess(BufferDescription& description, ResourceAccessFlags writeFlags)
{
    ASSERT(!enumHasAny(writeFlags,
        ResourceAccessFlags::Blit | ResourceAccessFlags::Sampled), "Buffer write has inappropriate flags")
    ASSERT(!enumHasAny(writeFlags, ResourceAccessFlags::Readback),
        "Cannot use readback flag in write operation")

    return applyWriteFlags(description, writeFlags, BUFFER_FLAGS_TO_SUB_ACCESS_MAP);
}

constexpr StageWithAccess inferBufferReadWriteAccess(BufferDescription& description, ResourceAccessFlags flags)
{
    ASSERT(!enumHasAny(flags,
        ResourceAccessFlags::Blit | ResourceAccessFlags::Sampled), "Buffer read-write has inappropriate flags")

    return applyReadWriteFlags(description, flags, BUFFER_FLAGS_TO_SUB_ACCESS_MAP);
}

constexpr StageWithAccess inferImageReadAccess(ImageDescription& description, ResourceAccessFlags readFlags)
{
    ASSERT(!enumHasAny(readFlags,
            ResourceAccessFlags::Attribute |
            ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform |
            ResourceAccessFlags::Readback),
        "Image read has inappropriate flags")
    ASSERT(
        enumHasAny(readFlags, ResourceAccessFlags::Blit | ResourceAccessFlags::Copy) ||
        enumHasAny(readFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
        enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
        "Image read does not have appropriate flags")

    return applyReadFlags(description, readFlags, IMAGE_FLAGS_TO_SUB_ACCESS_MAP);
}

constexpr StageWithAccess inferImageWriteAccess(ImageDescription& description, ResourceAccessFlags writeFlags)
{
    ASSERT(!enumHasAny(writeFlags,
            ResourceAccessFlags::Attribute |
            ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform),
        "Image write has inappropriate flags")
    ASSERT(
        enumHasAny(writeFlags, ResourceAccessFlags::Blit | ResourceAccessFlags::Copy) ||
        enumHasAny(writeFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
        enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
        "Image write does not have appropriate flags")
    ASSERT(!enumHasAny(writeFlags, ResourceAccessFlags::Readback),
        "Cannot use readback flag in write operation")

    return applyWriteFlags(description, writeFlags, IMAGE_FLAGS_TO_SUB_ACCESS_MAP);
}

constexpr StageWithAccess inferImageReadWriteAccess(ImageDescription& description, ResourceAccessFlags flags)
{
    ASSERT(!enumHasAny(flags,
            ResourceAccessFlags::Attribute |
            ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform),
        "Image read-write has inappropriate flags")
    ASSERT(
        enumHasAny(flags, ResourceAccessFlags::Blit | ResourceAccessFlags::Copy) ||
        enumHasAny(flags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
        enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
        "Image read-write does not have appropriate flags")

    return applyReadWriteFlags(description, flags, IMAGE_FLAGS_TO_SUB_ACCESS_MAP);
}
}

Graph::Graph(const std::array<DescriptorArenaAllocators, BUFFERED_FRAMES>& descriptorAllocators,
    lux::ShaderAssetManager& shaderAssetManager)
    : m_ArenaAllocators(descriptorAllocators), m_ShaderAssetManager(&shaderAssetManager)
{
}

void Graph::SetDescriptorAllocators(
    const std::array<DescriptorArenaAllocators, BUFFERED_FRAMES>& descriptorAllocators)
{
    m_ArenaAllocators = descriptorAllocators;
}

void Graph::SetWatcher(GraphWatcher& watcher)
{
    m_GraphWatcher = &watcher;
}

void Graph::RemoveWatcher()
{
    m_GraphWatcher = nullptr;
}

void Graph::OnFrameBegin(FrameContext& frameContext)
{
    m_FrameAllocators = &m_ArenaAllocators[frameContext.FrameNumber];
    frameContext.CommandList.BindDescriptorArenaAllocators({.Allocators = m_FrameAllocators});
    if (frameContext.FrameNumberTick >= BUFFERED_FRAMES)
        m_FrameAllocators->ResetTransient();
}

void Graph::Compile(FrameContext& frameContext)
{
    CPU_PROFILE_FRAME("Render Graph Compile")

    m_FrameDeletionQueue = &frameContext.DeletionQueue;

    auto dependencyList = BuildDependencyList();
    TopologicalSort(dependencyList);

    ProcessVirtualResources();
    PreProcessPersistentResources();
    ValidateImportedResources();

    if (m_GraphWatcher)
    {
        m_GraphWatcher->OnPassOrderFinalized(m_Passes);
        m_GraphWatcher->OnBufferResourcesFinalized(m_Buffers);
        m_GraphWatcher->OnImageResourcesFinalized(m_Images);
        m_GraphWatcher->OnBufferAccessesFinalized(m_BufferAccesses);
        m_GraphWatcher->OnImagesAccessesFinalized(m_ImageAccesses);
    }

    const auto bufferConflicts = FindBufferResourceConflicts();
    const auto imageConflicts = FindImageResourceConflicts();
    ManageBarriers(bufferConflicts, imageConflicts);

    PostProcessPersistentResources();
}

void Graph::Execute(FrameContext& frameContext)
{
    for (u32 i = 0; i < m_Passes.size(); i++)
    {
        m_PassIndicesStack.push_back(i);

        auto& pass = *m_Passes[i];
        if (IsPassSplitOrMerge(pass))
            continue;
        
        CMD_EXECUTION_LABEL(frameContext.Cmd, pass.Name().AsStringView());

        /* submit everything gathered at `setup` stage */
        SubmitPassUploads(frameContext);

        for (auto& barrier : pass.m_BarriersToWait)
            frameContext.CommandList.WaitOnBarrier({.DependencyInfo = barrier});
        for (auto& splitWait : pass.m_SplitBarriersToWait)
            frameContext.CommandList.WaitOnSplitBarrier({
                .SplitBarrier = splitWait.Barrier, .DependencyInfo = splitWait.Dependency
            });

        /* update layouts */
        for (auto& [image, layout] : pass.m_ImageLayouts)
            m_Images[image.m_Index].Layout = layout;

        if (enumHasAny(pass.m_Flags, PassFlags::Rasterization))
        {
            glm::uvec2 resolution = pass.m_RenderTargets.empty() ?
                GetImageDescription(pass.m_DepthStencilTargetAccess.Resource).Dimensions() :
                GetImageDescription(pass.m_RenderTargets.front().Resource).Dimensions();

            std::optional<DepthBias> depthBias{};

            std::vector<RenderingAttachment> colorAttachments;
            std::optional<RenderingAttachment> depthAttachment;
            colorAttachments.reserve(pass.m_RenderTargets.size());
            for (auto& target : pass.m_RenderTargets)
            {
                colorAttachments.push_back(Device::CreateRenderingAttachment({
                        .Description = ColorAttachmentDescription{
                            .Subresource = GetImageSubresourceDescription(target.Resource),
                            .OnLoad = target.Description.OnLoad,
                            .OnStore = target.Description.OnStore,
                            .ClearColor = target.Description.ClearColor
                        },
                        .Image = m_Images[target.Resource.m_Index].Resource,
                        .Layout = m_Images[target.Resource.m_Index].Layout
                    },
                    *m_FrameDeletionQueue));
            }
            if (pass.m_DepthStencilTargetAccess.Resource.IsValid())
            {
                auto& target = pass.m_DepthStencilTargetAccess;

                depthAttachment = Device::CreateRenderingAttachment({
                        .Description = DepthStencilAttachmentDescription{
                            .Subresource = GetImageSubresourceDescription(target.Resource),
                            .OnLoad = target.Description.OnLoad,
                            .OnStore = target.Description.OnStore,
                            .ClearDepthStencil = target.Description.ClearDepthStencil
                        },
                        .Image = m_Images[target.Resource.m_Index].Resource,
                        .Layout = m_Images[target.Resource.m_Index].Layout
                    },
                    *m_FrameDeletionQueue);

                if (target.DepthBias.has_value())
                    depthBias = *target.DepthBias;
            }

            frameContext.CommandList.BeginRendering({
                .RenderingInfo = Device::CreateRenderingInfo({
                        .RenderArea = resolution,
                        .ColorAttachments = colorAttachments,
                        .DepthAttachment = depthAttachment
                    },
                    *m_FrameDeletionQueue)
            });

            /* set dynamic states */
            frameContext.CommandList.SetViewport({.Size = resolution});
            frameContext.CommandList.SetScissors({.Size = resolution});
            if (depthBias.has_value())
                frameContext.CommandList.SetDepthBias({
                    .Constant = depthBias->Constant, .Slope = depthBias->Slope
                });

            if (!enumHasAny(pass.m_Flags, PassFlags::Disabled))
                pass.Execute(frameContext, *this);

            frameContext.CommandList.EndRendering({});
        }
        else
        {
            if (!enumHasAny(pass.m_Flags, PassFlags::Disabled))
                pass.Execute(frameContext, *this);
        }

        for (auto& splitSignal : pass.m_SplitBarriersToSignal)
            frameContext.CommandList.SignalSplitBarrier({
                .SplitBarrier = splitSignal.Barrier, .DependencyInfo = splitSignal.Dependency
            });

        m_PassIndicesStack.pop_back();
    }

    if (!m_Backbuffer.IsValid())
        return;

    /* transition backbuffer to the ColorAttachment for imgui render */
    const auto& backbuffer = m_Images[m_Backbuffer.m_Index];
    const ImageSubresource backbufferSubresource = {
        .Image = backbuffer.Resource,
        .Description = {.Mipmaps = 1, .Layers = 1}
    };
    LayoutTransitionInfo backbufferTransition = {
        .ImageSubresource = backbufferSubresource,
        .SourceStage = PipelineStage::AllCommands,
        .DestinationStage = PipelineStage::ColorOutput,
        .SourceAccess = PipelineAccess::WriteAll | PipelineAccess::ReadAll,
        .DestinationAccess = PipelineAccess::WriteColorAttachment,
        .OldLayout = backbuffer.Layout,
        .NewLayout = ImageLayout::ColorAttachment
    };
    frameContext.CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
            .LayoutTransitionInfo = backbufferTransition
        }, *m_FrameDeletionQueue)
    });
}

void Graph::Reset()
{
    m_Buffers.clear();
    m_Images.clear();
    m_BufferAccesses.clear();
    m_ImageAccesses.clear();
    m_Passes.clear();
    m_PassIndicesStack.clear();
    m_ResourcesPool.OnFrameEnd();
    ResetPersistentResources();

    if (m_GraphWatcher)
        m_GraphWatcher->OnReset();
}

std::vector<std::vector<u32>> Graph::BuildDependencyList() const
{
    CPU_PROFILE_FRAME("Build dependency list")

    const u32 totalResourceCount = u32(m_Buffers.size() + m_Images.size());
    auto resourceIndex = []<typename Res>(const Res resource) -> u64
    {
        const u64 baseIndex = (u64)(resource.m_Version << 15u | resource.m_Index) << 31u;
        
        if constexpr(std::is_same_v<Res, BufferResource>) 
            return baseIndex;
        else 
            return 1llu << 63u | baseIndex | (u64)resource.m_Extra;
    };
    std::unordered_multimap<u64, u32> consumers;
    std::unordered_map<u64, u32> producers;
    producers.reserve(totalResourceCount);
    consumers.reserve(m_BufferAccesses.size() + m_Images.size());

    /* A resource is produced by `Write` access, or by `Split` and `Merge` in case of images */
    auto findProducersConsumers = [&consumers, &producers, &resourceIndex, this]<typename Access>(
        const std::vector<Access>& accesses)
    {
        for (auto& access : accesses)
        {
            auto& info = access.Info;
            const u64 index = resourceIndex(access.Resource);
            if (info.HasRead())
                consumers.emplace(index, info.PassIndex);

            if (info.HasWrite() || info.IsSplitOrMerge())
            {
                if (info.HasWrite() && producers.contains(index))
                {
                    StringId name{};
                    if constexpr(std::is_same_v<Access, BufferResourceAccess>)
                        name = m_Buffers[access.Resource.m_Index].Name;
                    else
                        name = m_Images[access.Resource.m_Index].Name;
                    ASSERT(false, "Conflicting writes of resource '{}' between passes '{}' and '{}'",
                        name,
                        m_Passes[producers.at(index)]->m_Name.AsString(),
                        m_Passes[info.PassIndex]->m_Name.AsString()
                    )
                }
                producers.emplace(index, info.PassIndex);
            }
        }
    };

    {
        CPU_PROFILE_FRAME("Find producers and consumers")
        findProducersConsumers(m_BufferAccesses);
        findProducersConsumers(m_ImageAccesses);
    }

    std::vector dependencyList(m_Passes.size(), std::vector<u32>());
    for (auto& list : dependencyList)
        list.reserve(m_Passes.size());

    auto findDependencies = [&dependencyList, &consumers, &producers, &resourceIndex](auto& accesses)
    {
        auto addIfNew = [](auto& dependencies, u32 index)
        {
            if (std::ranges::find(dependencies, index) == dependencies.end())
                dependencies.push_back(index);
        };

        for (auto& access : accesses)
        {
            auto& info = access.Info;
            const u64 index = resourceIndex(access.Resource);
            if (info.HasWrite() || info.IsSplitOrMerge())
            {
                auto readResource = access.Resource;
                readResource.m_Version -= 1;
                const u64 readIndex = resourceIndex(readResource);
                if (consumers.contains(readIndex))
                {
                    auto range = consumers.equal_range(readIndex);
                    for (auto it = range.first; it != range.second; ++it)
                        if (it->second != info.PassIndex)
                            addIfNew(dependencyList[it->second], info.PassIndex);
                }
                if (producers.contains(readIndex) && producers.at(readIndex) != info.PassIndex)
                    addIfNew(dependencyList[producers.at(readIndex)], info.PassIndex);
            }
            if (!producers.contains(index) || producers.at(index) == info.PassIndex)
                continue;

            addIfNew(dependencyList[producers.at(index)], info.PassIndex);
        }
    };

    {
        CPU_PROFILE_FRAME("Find dependencies")
        findDependencies(m_BufferAccesses);
        findDependencies(m_ImageAccesses);
    }

    return dependencyList;
}

void Graph::TopologicalSort(std::vector<std::vector<u32>>& dependencyList)
{
    CPU_PROFILE_FRAME("Topological sort")

    struct Mark
    {
        bool Permanent{false};
        bool Temporary{false};
    };
    std::vector<Mark> marks(m_Passes.size());
    std::vector<u32> topologicalOrder;
    topologicalOrder.reserve(m_Passes.size());

    auto dfsSort = [&](u32 index)
    {
        auto dfsSortRecursive = [&marks, &topologicalOrder, &dependencyList, this](u32 index, auto& dfs)
        {
            if (marks[index].Permanent)
                return;
            ASSERT(!marks[index].Temporary, "Circular dependency in graph (pass {})", m_Passes[index]->Name())

            marks[index].Temporary = true;
            for (u32 adjacent : dependencyList[index])
                dfs(adjacent, dfs);
            marks[index].Temporary = false;
            marks[index].Permanent = true;
            topologicalOrder.push_back(index);
        };

        dfsSortRecursive(index, dfsSortRecursive);
    };

    for (u32 i = 0; i < m_Passes.size(); i++)
        if (!marks[i].Permanent)
            dfsSort(i);

    std::vector depths(m_Passes.size(), 0u);
    for (u32 parent : std::views::reverse(topologicalOrder))
        for (u32 child : dependencyList[parent])
            depths[child] = std::max(depths[child], depths[parent] + 1);

    DepthRetopology(depths, topologicalOrder);

    std::vector passRemap(topologicalOrder.size(), 0u);
    for (u32 i = 0; i < topologicalOrder.size(); i++)
        passRemap[topologicalOrder[i]] = i;

    /* permute according to the topology order */

    auto sortAndMergeAccesses = [&passRemap]<typename Access>(std::vector<Access>& accesses)
    {
        for (auto& access : accesses)
            access.Info.PassIndex = passRemap[access.Info.PassIndex];
        if (accesses.size() <= 1)
            return;

        std::ranges::stable_sort(accesses, std::less{}, [](const Access& access) -> u64
        {
            return ((u64)access.Info.PassIndex << 32) | access.Resource.m_Index;
        });
        std::vector<Access> merged;
        merged.reserve(accesses.size());
        merged.push_back(accesses.front());
        for (u32 i = 1; i < accesses.size(); i++)
        {
            auto& info = accesses[i].Info;
            auto& mergedInfo = merged.back().Info;
            
            bool canMerge = 
                accesses[i].Resource.m_Index == merged.back().Resource.m_Index &&
                info.PassIndex == mergedInfo.PassIndex;
            
            if constexpr(std::is_same_v<Access, ImageResourceAccess>)
                canMerge = 
                    canMerge && 
                    accesses[i].Resource.m_Extra == merged.back().Resource.m_Extra &&
                    !enumHasAny(mergedInfo.Type, AccessType::Split | AccessType::Merge);
            
            if (canMerge)
            {
                mergedInfo.Type |= info.Type;
                mergedInfo.Stage |= info.Stage;
                mergedInfo.Access |= info.Access;
            }
            else
            {
                merged.push_back(accesses[i]);
            }
        }
        accesses.swap(merged);
    };
    sortAndMergeAccesses(m_BufferAccesses);
    sortAndMergeAccesses(m_ImageAccesses);

    /* first, fix inner lists of dependencies as this does not require any swapping */
    for (auto& list : dependencyList)
        for (u32& pass : list)
            pass = passRemap[pass];

    /* now do the actual permutation of render passes and dependency list */
    for (u32 index = 0; index < topologicalOrder.size(); index++)
    {
        u32 current = index;
        u32 next = topologicalOrder[current];
        while (next != index)
        {
            std::swap(m_Passes[current], m_Passes[next]);
            std::swap(dependencyList[current], dependencyList[next]);
            topologicalOrder[current] = current;
            current = next;
            next = topologicalOrder[next];
        }
        topologicalOrder[current] = current;
    }
}

void Graph::DepthRetopology(const std::vector<u32>& depths, std::vector<u32>& topologicalOrder) const
{
    if (depths.size() < 2)
        return;

    std::vector<u32> depthBuckets;
    depthBuckets.resize(depths.size(), 0u);
    for (u32 depth : depths)
        depthBuckets[depth]++;

    u32 previous = depthBuckets[0];
    depthBuckets[0] = 0;
    for (u32 i = 1; i < depthBuckets.size(); i++)
    {
        const u32 current = depthBuckets[i];
        depthBuckets[i] = previous + depthBuckets[i - 1];
        previous = current;
    }

    for (u32 passIndex = 0; passIndex < depths.size(); passIndex++)
    {
        topologicalOrder[depthBuckets[depths[passIndex]]] = passIndex;
        depthBuckets[depths[passIndex]] += 1;
    }
}

void Graph::ProcessVirtualResources()
{
    CPU_PROFILE_FRAME("Process virtual resources")

    auto allocateResources = [this](auto& accesses, auto& resources)
    {
        for (auto& access : std::views::reverse(accesses))
        {
            auto& info = access.Info;
            auto& resource = resources[access.Resource.m_Index];
            if (const auto res = ValidateAccess(access, resource); !res.has_value())
            {
                LUX_LOG_ERROR("Invalid access detected for pass {}. The pass will be skipped, reason: {}",
                    m_Passes[info.PassIndex]->Name(), res.error());
                m_Passes[info.PassIndex]->m_Flags |= PassFlags::Disabled;
                continue;
            }

            if (resource.LastAccess == ResourceBase::NO_ACCESS)
                resource.LastAccess = info.PassIndex;
            resource.FirstAccess = info.PassIndex;
        }

        for (auto& access : accesses)
        {
            auto& info = access.Info;
            const auto handle = access.Resource;
            auto& resource = resources[handle.m_Index];
            if (!resource.Resource.HasValue() && resource.FirstAccess != ResourceBase::NO_ACCESS &&
                !resource.IsExported)
            {
                auto&& [physicalResource, aliasedFrom] = m_ResourcesPool.Allocate(handle, resource.Description,
                    resource.FirstAccess, resource.LastAccess);
                resource.Resource = physicalResource;
                resource.AliasedFrom = aliasedFrom;

                if (resource.AliasedFrom.IsValid())
                    resource.Name = StringId("{}/{}", resource.Name, resources[aliasedFrom.m_Index].Name);
            }
            if (!resource.Resource.HasValue())
                continue;
            if constexpr (std::is_same_v<std::decay_t<decltype(resource.Resource)>, Buffer>)
                Device::NameBuffer(resource.Resource, resource.Name.AsStringView());
            else if constexpr (std::is_same_v<std::decay_t<decltype(resource.Resource)>, Image>)
                Device::NameImage(resource.Resource, resource.Name.AsStringView());
        }
    };

    allocateResources(m_BufferAccesses, m_Buffers);
    allocateResources(m_ImageAccesses, m_Images);
}

Graph::ValidateAccessResult Graph::ValidateAccessCommon(const ResourceAccessInfo& info)
{
    if (info.Stage == PipelineStage::None)
        return std::unexpected("Stage is not inferrable");
    if (info.Access == PipelineAccess::None)
        return std::unexpected("Access is not inferrable");
    
    return {};
}

Graph::ValidateAccessResult Graph::ValidateAccess(const BufferResourceAccess& access, const RGBuffer& buffer)
{
    auto common = ValidateAccessCommon(access.Info);
    if (!common.has_value())
        return std::unexpected(std::format("Invalid access for buffer {}: {}", buffer.Name, common.error()));
    
    if (m_Buffers[access.Resource.m_Index].Description.Usage == BufferUsage::None)
        return std::unexpected(std::format("Invalid access for buffer {}: Usage is not inferrable", buffer.Name));
    
    return {};
}

Graph::ValidateAccessResult Graph::ValidateAccess(const ImageResourceAccess& access, const RGImage& image)
{
    if (access.Info.IsSplitOrMerge())
        return {};
    
    auto common = ValidateAccessCommon(access.Info);
    if (!common.has_value())
        return std::unexpected(std::format("Invalid access for image {}: {}", image.Name, common.error()));
    
    if (m_Images[access.Resource.m_Index].Description.Usage == ImageUsage::None)
        return std::unexpected(std::format("Invalid access for image {}: Usage is not inferrable", image.Name));
    
    return {};
}

void Graph::ValidateImportedResources()
{
    /* this function checks that the description of external resources is
     * compatible with the description of virtual resource */
    for (auto& buffer : m_Buffers)
        if (buffer.IsImported)
            if (!enumHasAll(Device::GetBufferDescription(buffer.Resource).Usage, buffer.Description.Usage))
                LUX_LOG_WARN("Imported buffer was created with flags"
                " that are incompatible with the in-frame usage of this resource: {}", buffer.Name);
    for (auto& image : m_Images)
        if (image.IsImported)
            if (!enumHasAll(Device::GetImageDescription(image.Resource).Usage, image.Description.Usage))
                LUX_LOG_WARN("Imported image was created with flags"
                " that are incompatible with the in-frame usage of this resource: {}", image.Name);
}

std::vector<BufferResourceAccessConflict> Graph::FindBufferResourceConflicts()
{
    CPU_PROFILE_FRAME("Find buffer resource conflicts")

    std::vector<BufferResourceAccessConflict> bufferConflicts;
    bufferConflicts.reserve(m_BufferAccesses.size());

    std::vector currentBufferAccess(m_Buffers.size(), BufferResourceAccess{});

    for (auto& access : m_BufferAccesses)
    {
        auto& info = access.Info;
        const u32 index = access.Resource.m_Index;

        BufferResourceAccess currentAccess = currentBufferAccess[index];
        auto& currentInfo = currentAccess.Info;
        currentBufferAccess[index] = access;

        if (currentInfo.PassIndex == ResourceAccessInfo::NO_PASS)
        {
            if (m_Buffers[index].AliasedFrom.IsValid())
                currentAccess = currentBufferAccess[m_Buffers[index].AliasedFrom.m_Index];
            else
                continue;
        }

        if (currentInfo.IsReadOnly() && info.IsReadOnly())
            continue;

        BufferResourceAccessConflict conflict = {
            .Info = {
                .FirstPassIndex = currentInfo.PassIndex,
                .SecondPassIndex = info.PassIndex,
                .FirstStage = currentInfo.Stage,
                .SecondStage = info.Stage,
                .FirstAccess = currentInfo.Access,
                .SecondAccess = info.Access
            },
            .Resource = access.Resource,
        };

        if (currentInfo.IsReadOnly() && info.HasWrite())
            conflict.Info.Type = AccessConflictType::Execution;
        else
            conflict.Info.Type = AccessConflictType::Memory;

        bufferConflicts.push_back(conflict);
    }

    return bufferConflicts;
}

std::vector<ImageResourceAccessConflict> Graph::FindImageResourceConflicts()
{
    CPU_PROFILE_FRAME("Find image resource conflicts")

    auto inferDesiredLayout = [this](const ResourceAccessInfo& access, const ImageDescription& description)
    {
        if (enumHasAny(access.Access, PipelineAccess::ReadTransfer | PipelineAccess::WriteTransfer))
        {
            ASSERT(
                (enumHasAny(access.Access, PipelineAccess::ReadTransfer) ||
                    enumHasAny(access.Access, PipelineAccess::WriteTransfer)) &&
                !enumHasAll(access.Access, PipelineAccess::ReadTransfer | PipelineAccess::WriteTransfer),
                "Cannot mix transfer accesses with any other type of access")
            if (enumHasAny(access.Access, PipelineAccess::ReadTransfer))
                return ImageLayout::Source;
            return ImageLayout::Destination;
        }
        if (access.HasWrite())
        {
            if (enumHasAny(access.Stage, PipelineStage::ComputeShader))
                return ImageLayout::General;
            if (enumHasAny(access.Access, PipelineAccess::WriteColorAttachment))
                return ImageLayout::ColorAttachment;
            if (enumHasAny(access.Access, PipelineAccess::WriteDepthStencilAttachment))
                return description.Format == Format::D32_FLOAT ?
                    ImageLayout::DepthAttachment :
                    ImageLayout::DepthStencilAttachment;
            return ImageLayout::Attachment;
        }
        if (enumHasAny(access.Access, PipelineAccess::ReadDepthStencilAttachment) ||
            description.Format == Format::D32_FLOAT ||
            description.Format == Format::D24_UNORM_S8_UINT ||
            description.Format == Format::D32_FLOAT_S8_UINT)
            return description.Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly :
                ImageLayout::DepthStencilReadonly;

        return enumHasAny(description.Usage, ImageUsage::Sampled) ?
            ImageLayout::Readonly :
            ImageLayout::General;
    };
    auto updateImageStateOnPureMerge = [](RGImage& image) -> RGImageState
    {
        bool isDivergent = false;
        for (u32 i = 1; i < image.Extras.size(); i++)
            isDivergent |= image.Extras[i].Layout != image.Extras[i - 1].Layout;

        if (!isDivergent)
            image.Layout = image.Extras.front().Layout;

        return isDivergent ? RGImageState::Divergent : RGImageState::Merged;
    };

    std::vector<ImageResourceAccessConflict> imageConflicts;
    imageConflicts.reserve(m_ImageAccesses.size());

    std::vector currentImageAccess(m_Images.size(), ImageResourceAccess{});

    /* all images start in Merged (not diverged) state */
    for (auto& image : m_Images)
        image.State = RGImageState::Merged;

    for (auto& access : m_ImageAccesses)
    {
        auto& info = access.Info;
        const u32 index = access.Resource.m_Index;
        auto& image = m_Images[index];

        if (info.OfType(AccessType::Split))
            image.State = RGImageState::Split;
        else if (info.OfType(AccessType::Merge))
            image.State = info.HasReadOrWrite() ? RGImageState::Divergent : updateImageStateOnPureMerge(image);
        if (!info.HasReadOrWrite())
            continue;

        ImageResourceAccess currentAccess = currentImageAccess[index];
        auto& currentInfo = currentAccess.Info;
        currentImageAccess[index] = access;

        if (currentInfo.PassIndex == ResourceAccessInfo::NO_PASS)
            if (image.AliasedFrom.IsValid())
                currentAccess = currentImageAccess[image.AliasedFrom.m_Index];

        ImageResourceAccessConflict conflict = {
            .Info = {
                .FirstPassIndex = currentInfo.PassIndex,
                .SecondPassIndex = info.PassIndex,
                .FirstStage = currentInfo.Stage,
                .SecondStage = info.Stage,
                .FirstAccess = currentInfo.Access,
                .SecondAccess = info.Access
            },
            .Resource = access.Resource,
        };

        const bool isMainSubresource = access.Resource.m_Extra == ImageResource::NO_EXTRA;
        const ImageLayout currentLayout = isMainSubresource ?
            image.Layout :
            image.Extras[access.Resource.m_Extra].Layout;
        const ImageLayout newLayout = inferDesiredLayout(info, image.Description);

        if (image.State == RGImageState::Divergent)
        {
            image.State = RGImageState::Merged;
            if (HasChangedAllSplitLayouts(imageConflicts, conflict, image, newLayout))
                continue;
            image.Layout = newLayout;
        }

        if (currentLayout != newLayout)
        {
            if (isMainSubresource)
                ChangeMainImageLayout(imageConflicts, conflict, image, newLayout);
            else
                ChangeSubresourceImageLayout(imageConflicts, conflict, image, access.Resource.m_Extra, newLayout);
            continue;
        }

        if (currentInfo.PassIndex == ResourceAccessInfo::NO_PASS)
            continue;

        if (currentInfo.IsReadOnly() && info.IsReadOnly())
            continue;

        if (currentInfo.IsReadOnly() && info.HasWrite())
            conflict.Info.Type = AccessConflictType::Execution;
        else
            conflict.Info.Type = AccessConflictType::Memory;

        imageConflicts.push_back(conflict);
    }

    return imageConflicts;
}

bool Graph::HasChangedAllSplitLayouts(std::vector<ImageResourceAccessConflict>& conflicts,
    ImageResourceAccessConflict& conflict, RGImage& image, ImageLayout newLayout)
{
    /* every subresource might have its own layout */
    bool needLayoutChange = image.Extras.front().Layout != newLayout;
    bool isDivergent = false;
    for (u32 i = 1; i < image.Extras.size(); i++)
    {
        isDivergent |= image.Extras[i].Layout != image.Extras[i - 1].Layout;
        if (image.Extras[i].Layout != image.Extras[i - 1].Layout || image.Extras[i].Layout != newLayout)
            needLayoutChange = true;
    }

    if (!needLayoutChange)
        return false;

    if (!isDivergent)
    {
        image.Layout = image.Extras.front().Layout;
        ChangeMainImageLayout(conflicts, conflict, image, newLayout);
        return true;
    }

    /* we have at least a pair of subresources that diverged in layout */
    conflict.Info.Type = AccessConflictType::Layout;
    for (u32 i = 0; i < image.Extras.size(); i++)
    {
        if (image.Extras[i].Layout != newLayout)
        {
            ImageResourceAccessConflict layoutConflict = {
                .Info = conflict.Info,
                .Resource = conflict.Resource,
                .FirstLayout = image.Extras[i].Layout,
                .SecondLayout = newLayout,
                .Subresource = image.Description.AdditionalViews[i]
            };
            conflicts.push_back(layoutConflict);

            image.Extras[i].Layout = newLayout;
        }
    }
    image.Layout = newLayout;

    return true;
}

void Graph::ChangeMainImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
    ImageResourceAccessConflict& conflict, RGImage& image, ImageLayout newLayout)
{
    conflict.Info.Type = AccessConflictType::Layout;
    ImageResourceAccessConflict layoutConflict = {
        .Info = conflict.Info,
        .Resource = conflict.Resource,
        .FirstLayout = image.Layout,
        .SecondLayout = newLayout
    };
    conflicts.push_back(layoutConflict);

    for (auto& extra : image.Extras)
        extra.Layout = newLayout;
    image.Layout = newLayout;
}

void Graph::ChangeSubresourceImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
    ImageResourceAccessConflict& conflict, RGImage& image, u32 subresourceIndex, 
    ImageLayout newLayout)
{
    ASSERT(image.State == RGImageState::Split)

    conflict.Info.Type = AccessConflictType::Layout;
    ImageResourceAccessConflict layoutConflict = {
        .Info = conflict.Info,
        .Resource = conflict.Resource,
        .FirstLayout = image.Extras[subresourceIndex].Layout,
        .SecondLayout = newLayout,
        .Subresource = image.Description.AdditionalViews[subresourceIndex]
    };
    conflicts.push_back(layoutConflict);

    image.Extras[subresourceIndex].Layout = newLayout;
}

void Graph::ManageBarriers(const std::vector<BufferResourceAccessConflict>& bufferConflicts,
    const std::vector<ImageResourceAccessConflict>& imageConflicts)
{
    CPU_PROFILE_FRAME("Manage barriers")

    // todo: barrier merging

    auto addBarriers = [this]<typename Res>(DependencyInfoCreateInfo& dependencyInfo, Res resource,
        u32 firstPass, u32 secondPass)
    {
        const u32 span = secondPass - firstPass;
        if (span > 1 && firstPass != ResourceAccessInfo::NO_PASS)
        {
            if (m_GraphWatcher)
                m_GraphWatcher->OnBarrierAdded({
                    .Info = {
                        .BarrierType = GraphWatcher::BarrierInfo::Type::SplitBarrier,
                        .DependencyInfo = &dependencyInfo
                    },
                    .Resource = resource,
                }, *m_Passes[firstPass], *m_Passes[secondPass]);

            const DependencyInfo dependency = Device::CreateDependencyInfo(
                std::move(dependencyInfo), *m_FrameDeletionQueue);
            const SplitBarrier splitBarrier = Device::CreateSplitBarrier(*m_FrameDeletionQueue);
            m_Passes[firstPass]->m_SplitBarriersToSignal.push_back({
                .Dependency = dependency,
                .Barrier = splitBarrier
            });
            m_Passes[secondPass]->m_SplitBarriersToWait.push_back({
                .Dependency = dependency,
                .Barrier = splitBarrier
            });
        }
        else
        {
            if (m_GraphWatcher)
            {
                const GraphWatcher::BarrierInfo watcherBarrierInfo = {
                    .BarrierType = GraphWatcher::BarrierInfo::Type::Barrier,
                    .DependencyInfo = &dependencyInfo
                };
                if (firstPass == ResourceAccessInfo::NO_PASS)
                    m_GraphWatcher->OnBarrierAdded({.Info = watcherBarrierInfo, .Resource = resource},
                        *m_Passes[secondPass], *m_Passes[secondPass]);
                else
                    m_GraphWatcher->OnBarrierAdded({.Info = watcherBarrierInfo, .Resource = resource},
                        *m_Passes[firstPass], *m_Passes[secondPass]);
            }

            const DependencyInfo dependency = Device::CreateDependencyInfo(
                std::move(dependencyInfo), *m_FrameDeletionQueue);
            m_Passes[secondPass]->m_BarriersToWait.push_back(dependency);
        }
    };

    for (auto& buffer : bufferConflicts)
    {
        auto& info = buffer.Info;
        DependencyInfoCreateInfo dependencyCreateInfo = {};
        if (info.Type == AccessConflictType::Execution)
            dependencyCreateInfo.ExecutionDependencyInfo = {
                .SourceStage = info.FirstStage,
                .DestinationStage = info.SecondStage
            };
        else
            dependencyCreateInfo.MemoryDependencyInfo = {
                .SourceStage = info.FirstStage,
                .DestinationStage = info.SecondStage,
                .SourceAccess = info.FirstAccess,
                .DestinationAccess = info.SecondAccess
            };

        addBarriers(dependencyCreateInfo, buffer.Resource, info.FirstPassIndex, info.SecondPassIndex);
    }

    for (auto& image : imageConflicts)
    {
        auto& info = image.Info;
        DependencyInfoCreateInfo dependencyCreateInfo = {};
        if (info.Type == AccessConflictType::Layout)
        {
            dependencyCreateInfo.LayoutTransitionInfo = {
                .ImageSubresource = ImageSubresource{
                    .Image = m_Images[image.Resource.m_Index].Resource,
                    .Description = image.Subresource
                },
                .SourceStage = info.FirstStage,
                .DestinationStage = info.SecondStage,
                .SourceAccess = info.FirstAccess,
                .DestinationAccess = info.SecondAccess,
                .OldLayout = image.FirstLayout,
                .NewLayout = image.SecondLayout
            };

            m_Passes[info.SecondPassIndex]->m_ImageLayouts.push_back({
                .Image = image.Resource,
                .Layout = image.SecondLayout
            });
        }
        else if (info.Type == AccessConflictType::Execution)
        {
            dependencyCreateInfo.MemoryDependencyInfo = {
                .SourceStage = info.FirstStage,
                .DestinationStage = info.SecondStage,
            };
        }
        else
        {
            dependencyCreateInfo.MemoryDependencyInfo = {
                .SourceStage = info.FirstStage,
                .DestinationStage = info.SecondStage,
                .SourceAccess = info.FirstAccess,
                .DestinationAccess = info.SecondAccess
            };
        }

        addBarriers(dependencyCreateInfo, image.Resource, info.FirstPassIndex, info.SecondPassIndex);
    }
}

void Graph::PreProcessPersistentResources()
{
    for (auto& persistent : m_PersistentBuffers)
    {
        if (!persistent.Resource.IsValid())
            continue;
        auto& buffer = m_Buffers[persistent.Resource.m_Index];
        if (!persistent.Buffer.HasValue() && persistent.DeletionQueue != nullptr)
            persistent.Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = buffer.Description.SizeBytes,
                    .Usage = buffer.Description.Usage,
                },
            }, *persistent.DeletionQueue);

        buffer.Resource = persistent.Buffer;
        Device::NameBuffer(buffer.Resource, buffer.Name.AsStringView());
    }
    for (auto& persistent : m_PersistentImages)
    {
        if (!persistent.Resource.IsValid())
            continue;
        auto& image = m_Images[persistent.Resource.m_Index];
        if (!persistent.Image.HasValue() && persistent.DeletionQueue != nullptr)
            persistent.Image = Device::CreateImage({
                .Description = image.Description
            }, *persistent.DeletionQueue);

        image.Resource = persistent.Image;
        Device::NameImage(image.Resource, image.Name.AsStringView());
    }
}

void Graph::PostProcessPersistentResources()
{
    for (auto& persistent : m_PersistentImages)
    {
        if (!persistent.Resource.IsValid())
            continue;
        ASSERT(m_Images[persistent.Resource.m_Index].State != RGImageState::Divergent)
        persistent.Layout = m_Images[persistent.Resource.m_Index].Layout;
    }
}

void Graph::ResetPersistentResources()
{
    for (auto& persistent : m_PersistentBuffers)
        persistent.Resource = BufferResource{};
    for (auto& persistent : m_PersistentImages)
        persistent.Resource = ImageResource{};
}

void Graph::SubmitPassUploads(FrameContext& frameContext)
{
    /* avoid barriers if there is no data to upload */
    if (!m_ResourceUploader.HasUploads(CurrentPass()))
        return;

    frameContext.CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
                .ExecutionDependencyInfo = ExecutionDependencyInfo{
                    .SourceStage = PipelineStage::AllCommands,
                    .DestinationStage = PipelineStage::AllTransfer
                }
            },
            frameContext.DeletionQueue)
    });

    m_ResourceUploader.Upload(CurrentPass(), *this, *frameContext.ResourceUploader);
    frameContext.ResourceUploader->SubmitUpload(frameContext);

    frameContext.CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
                .MemoryDependencyInfo = MemoryDependencyInfo{
                    .SourceStage = PipelineStage::AllTransfer,
                    .DestinationStage = PipelineStage::AllCommands,
                    .SourceAccess = PipelineAccess::WriteAll,
                    .DestinationAccess = PipelineAccess::ReadAll
                }
            },
            frameContext.DeletionQueue)
    });
}

BufferResource Graph::Create(StringId name, const RGBufferDescription& description)
{
    ASSERT(description.SizeBytes > 0)

    const BufferResource resource(ResourceFlags::None, (u16)m_Buffers.size(), 0);
    RGBuffer buffer = {};
    buffer.Name = name;
    buffer.Description = CreateBufferDescription(description);
    m_Buffers.push_back(buffer);

    return resource;
}

ImageResource Graph::Create(StringId name, const RGImageDescription& description)
{
    return Create(name, ResourceCreationFlags::None, description);
}

ImageResource Graph::Create(StringId name, ResourceCreationFlags creationFlags, const RGImageDescription& description)
{
    const ImageResource resource((ResourceFlags)(creationFlags), (u16)m_Images.size(), 0);
    RGImage image = {};
    image.Name = name;
    image.Description = CreateImageDescription(description);
    if (enumHasAny(description.Inference, RGImageInference::Views))
        image.Extras = m_Images[description.Reference.m_Index].Extras;
    m_Images.push_back(image);

    return resource;
}

PersistentBufferResource Graph::AddPersistent(Buffer buffer)
{
    PersistentBufferResource resource = {};

    auto it = std::ranges::find_if(m_PersistentBuffers, [buffer](const auto& persistent)
    {
        return persistent.Buffer == buffer;
    });

    if (it != m_PersistentBuffers.end())
    {
        const u32 index = (u32)(it - m_PersistentBuffers.begin());
        resource.m_Index = index;
    }
    else
    {
        resource.m_Index = (u32)m_PersistentBuffers.size();
        m_PersistentBuffers.push_back({.Buffer = buffer});
    }


    return resource;
}

PersistentImageResource Graph::AddPersistent(Image image, ImageLayout layout)
{
    PersistentImageResource resource = {};

    auto it = std::ranges::find_if(m_PersistentImages, [image](const auto& persistent)
    {
        return persistent.Image == image;
    });

    if (it != m_PersistentImages.end())
    {
        const u32 index = (u32)(it - m_PersistentImages.begin());
        resource.m_Index = index;
    }
    else
    {
        resource.m_Index = (u32)m_PersistentImages.size();
        m_PersistentImages.push_back({.Image = image, .Layout = layout});
    }

    return resource;
}

void Graph::UpdatePersistent(PersistentBufferResource resource, Buffer buffer)
{
    m_PersistentBuffers[resource.m_Index].Buffer = buffer;
}

void Graph::UpdatePersistent(PersistentImageResource resource, Image image)
{
    m_PersistentImages[resource.m_Index].Image = image;
}

void Graph::UpdatePersistent(PersistentImageResource resource, Image image, ImageLayout layout)
{
    m_PersistentImages[resource.m_Index].Image = image;
    m_PersistentImages[resource.m_Index].Layout = layout;
}

BufferResource Graph::ImportPersistent(StringId name, PersistentBufferResource buffer)
{
    PersistentBufferInfo& persistentBufferInfo = m_PersistentBuffers[buffer.m_Index];
    if (persistentBufferInfo.Resource.IsValid())
        return persistentBufferInfo.Resource;

    const BufferResource resource = Import(name, persistentBufferInfo.Buffer);
    persistentBufferInfo.Resource = resource;

    return resource;
}

ImageResource Graph::ImportPersistent(StringId name, PersistentImageResource image)
{
    PersistentImageInfo& persistentImageInfo = m_PersistentImages[image.m_Index];
    if (persistentImageInfo.Resource.IsValid())
        return persistentImageInfo.Resource;

    const ImageResource resource = Import(name, persistentImageInfo.Image, persistentImageInfo.Layout);
    persistentImageInfo.Resource = resource;

    return resource;
}

BufferResource Graph::Import(StringId name, Buffer buffer)
{
    const BufferResource resource(ResourceFlags::Imported, (u16)m_Buffers.size(), 0);
    RGBuffer bufferResource = {};
    bufferResource.Name = name;
    bufferResource.Description = Device::GetBufferDescription(buffer);
    bufferResource.Resource = buffer;
    m_Buffers.push_back(bufferResource);

    return resource;
}

ImageResource Graph::Import(StringId name, Image image, ImageLayout layout)
{
    const ImageResource resource(ResourceFlags::Imported, (u16)m_Images.size(), 0);
    RGImage imageResource = {};
    imageResource.Name = name;
    imageResource.Description = Device::GetImageDescription(image);
    imageResource.Resource = image;
    imageResource.Layout = layout;
    imageResource.Extras.resize(imageResource.Description.AdditionalViews.size(), RGImageExtraInfo{
        .Version = 0,
        .Layout = layout
    });
    m_Images.push_back(imageResource);

    return resource;
}

void Graph::Export(BufferResource resource, PersistentBufferResource& buffer, DeletionQueue& deletionQueue,
    BufferUsage additionalUsage)
{
    if (!buffer.HasValue())
    {
        buffer.m_Index = (u32)m_PersistentBuffers.size();
        m_PersistentBuffers.push_back({});
    }

    RG_CHECK_RETURN_VOID(!resource.HasFlags(ResourceFlags::Volatile | ResourceFlags::Imported),
        "Provided resource is not an internal buffer")
    PersistentBufferInfo& persistentBufferInfo = m_PersistentBuffers[buffer.m_Index];
    RG_CHECK_RETURN_VOID(
        !persistentBufferInfo.Resource.IsValid() ||
        persistentBufferInfo.Resource == resource ||
        !m_Buffers[persistentBufferInfo.Resource.m_Index].IsExported,
        "Provided buffer already has a resource associated with it")

    m_Buffers[resource.m_Index].IsExported = true;
    m_Buffers[resource.m_Index].Description.Usage |= additionalUsage;
    persistentBufferInfo.Resource = resource;
    persistentBufferInfo.DeletionQueue = &deletionQueue;
}

void Graph::Export(ImageResource resource, PersistentImageResource& image, DeletionQueue& deletionQueue,
    ImageUsage additionalUsage)
{
    if (!image.HasValue())
    {
        image.m_Index = (u32)m_PersistentImages.size();
        m_PersistentImages.push_back({});
    }

    RG_CHECK_RETURN_VOID(!resource.HasFlags(ResourceFlags::Volatile | ResourceFlags::Imported),
        "Provided resource is not an internal image")
    PersistentImageInfo& persistentImageInfo = m_PersistentImages[image.m_Index];
    RG_CHECK_RETURN_VOID(
        !persistentImageInfo.Resource.IsValid() ||
        persistentImageInfo.Resource == resource,
        "Provided image already has a resource associated with it")

    m_Images[resource.m_Index].IsExported = true;
    m_Images[resource.m_Index].Layout = persistentImageInfo.Layout;
    for (auto& extra : m_Images[resource.m_Index].Extras)
        extra.Layout = persistentImageInfo.Layout;
    m_Images[resource.m_Index].Description.Usage |= additionalUsage;
    persistentImageInfo.Resource = resource;
    persistentImageInfo.DeletionQueue = &deletionQueue;
}

ImageResource Graph::SplitImage(ImageResource main, ImageSubresourceDescription subresource)
{
    auto& image = m_Images[main.m_Index];

    ImageResource split = main;
    u16 subresourceIndex = {};
    auto it = std::ranges::find(image.Description.AdditionalViews, subresource);
    if (it == image.Description.AdditionalViews.end())
    {
        RG_CHECK_RETURN(!main.HasFlags(ResourceFlags::Imported),
            "Failed to split image resource: {}. "
            "Resource have to be internal image in order to have additional views", main)

        subresourceIndex = (u16)image.Description.AdditionalViews.size();
        RG_CHECK_RETURN(subresourceIndex < std::numeric_limits<u8>::max() - 1,
            "Failed to split image resource: {}. Too many splits exist already", main)
        image.Description.AdditionalViews.push_back(subresource);
        image.Extras.push_back({
            .Version = split.m_Version,
            .Layout = ImageLayout::Undefined
        });
    }
    else
    {
        subresourceIndex = (u16)std::distance(image.Description.AdditionalViews.begin(), it);
    }

    split.m_Flags &= ~ResourceFlags::Merge;
    split.AddFlags(ResourceFlags::Split);
    split.m_Extra = (u8)subresourceIndex;
    image.State = RGImageState::Split;
    image.ActiveSplitCount += 1;

    /* this uses separate pass instead of directly calling `ReadImage` to allow for split outside Pass */
    if (m_PassIndicesStack.empty())
    {
        AddRenderPass<std::nullptr_t>(m_SplitPassName,
            [&](Graph& graph, std::nullptr_t&)
            {
                graph.AddImageAccess(main, AccessType::Split, image, PipelineStage::None, PipelineAccess::None);
                split = graph.AddImageAccess(split, AccessType::Split, image, PipelineStage::None,
                    PipelineAccess::None);
            },
            [=](const std::nullptr_t&, FrameContext&, const Graph&)
            {
            });
    }
    else
    {
        AddImageAccess(main, AccessType::Split, image, PipelineStage::None, PipelineAccess::None);
        split = AddImageAccess(split, AccessType::Split, image, PipelineStage::None, PipelineAccess::None);
    }

    return split;
}

ImageResource Graph::MergeImage(Span<const ImageResource> splits)
{
    RG_CHECK_RETURN(!splits.empty(), "Failed to merge image: nothing to merge")

    const u16 index = splits.front().m_Index;
    auto& image = m_Images[index];

    for (const ImageResource split : splits | std::views::drop(1))
    {
        RG_CHECK_RETURN(split.m_Index == index, "Failed to merge image: split indices do not match")
        RG_CHECK_RETURN(split.HasFlags(ResourceFlags::Split),
            "Failed to merge image: resource is not a split {}", split)
    }
    RG_CHECK_RETURN(splits.size() == image.ActiveSplitCount, "Failed to merge image: must merge on all splits")

    ImageResource merged = splits.front();
    merged.m_Flags &= ~ResourceFlags::Split;
    merged.AddFlags(ResourceFlags::Merge);
    merged.m_Version = image.LatestVersion;
    merged.m_Extra = ImageResource::NO_EXTRA;
    image.State = RGImageState::Merged;
    image.ActiveSplitCount -= (u16)splits.size();

    AddRenderPass<std::nullptr_t>(m_MergePassName,
        [&](Graph& graph, std::nullptr_t&)
        {
            merged = graph.AddImageAccess(merged, AccessType::Merge, image, PipelineStage::None, PipelineAccess::None);
            for (const ImageResource split : splits)
                graph.AddImageAccess(split, AccessType::Merge, image, PipelineStage::None, PipelineAccess::None);
        },
        [=](const std::nullptr_t&, FrameContext&, const Graph&)
        {
        });

    return merged;
}

BufferResource Graph::ReadBuffer(BufferResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferBufferReadAccess(m_Buffers[resource.m_Index].Description, accessFlags);

    return AddBufferAccess(resource, AccessType::Read, m_Buffers[resource.m_Index], stage, access);
}

BufferResource Graph::WriteBuffer(BufferResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferBufferWriteAccess(m_Buffers[resource.m_Index].Description, accessFlags);

    return AddBufferAccess(resource, AccessType::Write, m_Buffers[resource.m_Index], stage, access);
}

BufferResource Graph::ReadWriteBuffer(BufferResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferBufferReadWriteAccess(m_Buffers[resource.m_Index].Description, accessFlags);

    return AddBufferAccess(resource, AccessType::ReadWrite, m_Buffers[resource.m_Index], stage, access);
}

ImageResource Graph::ReadImage(ImageResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferImageReadAccess(m_Images[resource.m_Index].Description, accessFlags);

    return AddImageAccess(resource, AccessType::Read, m_Images[resource.m_Index], stage, access);
}

ImageResource Graph::WriteImage(ImageResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferImageWriteAccess(m_Images[resource.m_Index].Description, accessFlags);

    return AddImageAccess(resource, AccessType::Write, m_Images[resource.m_Index], stage, access);
}

ImageResource Graph::ReadWriteImage(ImageResource resource, ResourceAccessFlags accessFlags)
{
    auto&& [stage, access] = inferImageReadWriteAccess(m_Images[resource.m_Index].Description, accessFlags);

    return AddImageAccess(resource, AccessType::ReadWrite, m_Images[resource.m_Index], stage, access);
}

ImageResource Graph::RenderTarget(ImageResource resource, const RenderTargetAccessDescription& description)
{
    m_Images[resource.m_Index].Description.Usage |= ImageUsage::Color;

    if (description.OnLoad == AttachmentLoad::Load)
        resource = AddImageAccess(resource, AccessType::Read, m_Images[resource.m_Index],
            PipelineStage::ColorOutput, PipelineAccess::ReadColorAttachment);
    if (description.OnStore == AttachmentStore::Store)
        resource = AddImageAccess(resource, AccessType::Write, m_Images[resource.m_Index],
            PipelineStage::ColorOutput, PipelineAccess::WriteColorAttachment);

    CurrentPass().m_RenderTargets.push_back({
        .Resource = resource,
        .Description = description
    });
    CurrentPass().m_Flags |= PassFlags::Rasterization;

    return resource;
}

ImageResource Graph::DepthStencilTarget(ImageResource resource, const DepthStencilTargetAccessDescription& description,
    std::optional<DepthBias> depthBias)
{
    m_Images[resource.m_Index].Description.Usage |= ImageUsage::Depth | ImageUsage::Stencil;

    if (description.OnLoad == AttachmentLoad::Load)
        resource = AddImageAccess(resource, AccessType::Read, m_Images[resource.m_Index],
            PipelineStage::DepthLate, PipelineAccess::ReadDepthStencilAttachment);
    if (description.OnStore == AttachmentStore::Store)
        resource = AddImageAccess(resource, AccessType::Write, m_Images[resource.m_Index],
            PipelineStage::DepthLate, PipelineAccess::WriteDepthStencilAttachment);

    CurrentPass().m_DepthStencilTargetAccess = {
        .Resource = resource,
        .Description = description,
        .DepthBias = depthBias
    };
    CurrentPass().m_Flags |= PassFlags::Rasterization;

    return resource;
}

void Graph::HasSideEffect() const
{
    CurrentPass().m_Flags &= ~PassFlags::Cullable;
}

ImageResource Graph::SetBackbufferImage(Image backbuffer, ImageLayout layout)
{
    m_Backbuffer = Import("Backbuffer"_hsv, backbuffer, layout);
    m_Backbuffer.AddFlags(ResourceFlags::AutoUpdate);

    return m_Backbuffer;
}

ImageResource Graph::GetBackbufferImage() const
{
    return m_Backbuffer;
}

const BufferDescription& Graph::GetBufferDescription(BufferResource buffer) const
{
    return m_Buffers[buffer.m_Index].Description;
}

const ImageDescription& Graph::GetImageDescription(ImageResource image) const
{
    return m_Images[image.m_Index].Description;
}

Buffer Graph::GetBuffer(BufferResource buffer) const
{
    return m_Buffers[buffer.m_Index].Resource;
}

Image Graph::GetImage(ImageResource image) const
{
    return m_Images[image.m_Index].Resource;
}

Buffer Graph::GetBuffer(PersistentBufferResource buffer) const
{
    if (!buffer.HasValue())
        return {};
    return m_PersistentBuffers[buffer.m_Index].Buffer;
}

Image Graph::GetImage(PersistentImageResource image) const
{
    if (!image.HasValue())
        return {};
    return m_PersistentImages[image.m_Index].Image;
}

std::pair<Buffer, const BufferDescription&> Graph::GetBufferWithDescription(BufferResource buffer) const
{
    return {m_Buffers[buffer.m_Index].Resource, m_Buffers[buffer.m_Index].Description};
}

std::pair<Image, const ImageDescription&> Graph::GetImageWithDescription(ImageResource image) const
{
    return {m_Images[image.m_Index].Resource, m_Images[image.m_Index].Description};
}

std::pair<Buffer, const BufferDescription&> Graph::GetBufferWithDescription(PersistentBufferResource buffer) const
{
    const auto& persistent = m_PersistentBuffers[buffer.m_Index];
    return {persistent.Buffer, m_Buffers[persistent.Resource.m_Index].Description};
}

std::pair<Image, const ImageDescription&> Graph::GetImageWithDescription(PersistentImageResource image) const
{
    const auto& persistent = m_PersistentImages[image.m_Index];
    return {persistent.Image, m_Images[persistent.Resource.m_Index].Description};
}

bool Graph::IsBufferAllocated(BufferResource buffer) const
{
    ASSERT(buffer.IsValid(), "Provided resource is not a valid buffer {}", buffer)

    return m_Buffers[buffer.m_Index].Resource.HasValue();
}

bool Graph::IsImageAllocated(ImageResource image) const
{
    ASSERT(image.IsValid(), "Provided resource is not a valid image {}", image)

    return m_Images[image.m_Index].Resource.HasValue();
}

BufferBinding Graph::GetBufferBinding(BufferResource buffer) const
{
    ASSERT(!m_PassIndicesStack.empty(), "This method should be called at pass execution stage")
    ASSERT(buffer.IsValid(), "Provided resource is not a valid buffer {}", buffer)

    return {.Buffer = m_Buffers[buffer.m_Index].Resource};
}

ImageBinding Graph::GetImageBinding(ImageResource image) const
{
    ASSERT(!m_PassIndicesStack.empty(), "This method should be called at pass execution stage")
    ASSERT(image.IsValid(), "Provided resource is not a valid image {}", image)

    return {
        .Subresource = {
            .Image = m_Images[image.m_Index].Resource,
            .Description = GetImageSubresourceDescription(image)
        },
        .Layout = m_Images[image.m_Index].Layout
    };
}

DescriptorArenaAllocators& Graph::GetFrameAllocators() const
{
    return *m_FrameAllocators;
}

Blackboard& Graph::GetBlackboard()
{
    return m_Blackboard;
}

const GlobalResources& Graph::GetGlobalResources() const
{
    return m_Blackboard.Get<GlobalResources>();
}

const lux::ShaderAsset& Graph::SetShader(StringId name) const
{
    return SetShader(name, std::nullopt, {});
}

const lux::ShaderAsset& Graph::SetShader(StringId name, StringId variant) const
{
    return SetShader(name, variant, {});
}

const lux::ShaderAsset& Graph::SetShader(StringId name, ShaderOverridesView&& overrides) const
{
    return SetShader(name, std::nullopt, std::move(overrides));
}

const lux::ShaderAsset& Graph::SetShader(StringId name, std::optional<StringId> variant,
    ShaderOverridesView&& overrides) const
{
    const lux::ShaderHandle shaderHandle = m_ShaderAssetManager->LoadResource({
        .Name = name,
        .Variant = variant,
        .Overrides = &overrides
    });
    if (!shaderHandle.IsValid())
        return HandleShaderError(name);

    const auto res = m_ShaderAssetManager->Allocate(shaderHandle, GetFrameAllocators());
    if (!res.has_value())
        return HandleShaderError(name);

    CurrentPass().m_Shader = m_ShaderAssetManager->Get(shaderHandle).value_or({});

    return GetShader();
}

const lux::ShaderAsset& Graph::HandleShaderError(StringId name) const
{
    static constexpr lux::ShaderAsset DUMMY = {};
    LUX_LOG_ERROR("Error while setting shader {}. Pass will be disabled", name);
    CurrentPass().m_Flags |= PassFlags::Disabled;
    return DUMMY;
}

const lux::ShaderAsset& Graph::GetShader() const
{
    return CurrentPass().m_Shader;
}

BufferDescription Graph::CreateBufferDescription(const RGBufferDescription& description) const
{
    return BufferDescription{.SizeBytes = description.SizeBytes};
}

ImageDescription Graph::CreateImageDescription(const RGImageDescription& description) const
{
    ImageDescription imageDescription = {
        .Width = (u32)description.Width,
        .Height = (u32)description.Height,
        .LayersDepth = (u32)description.LayersDepth,
        .Mipmaps = description.Mipmaps,
        .Format = description.Format,
        .Kind = description.Kind,
        .MipmapFilter = description.MipmapFilter
    };
    if (description.Inference == RGImageInference::None)
        return imageDescription;

    ASSERT(description.Reference.IsValid(), 
        "Provided reference resource is not a valid image {}", description.Reference)

    const ImageDescription& reference = m_Images[description.Reference.m_Index].Description;
    const bool wholeInference = description.Inference == RGImageInference::Full;
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Size2d))
    {
        imageDescription.Width = u32(description.Width * (f32)reference.Width);
        imageDescription.Height = u32(description.Height * (f32)reference.Height);
    }
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Depth))
        imageDescription.LayersDepth = u32(description.LayersDepth * (f32)reference.LayersDepth);
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Format))
        imageDescription.Format = reference.Format;
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Kind))
        imageDescription.Kind = reference.Kind;
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Views))
        imageDescription.AdditionalViews = reference.AdditionalViews;
    if (wholeInference || enumHasAny(description.Inference, RGImageInference::Filter))
        imageDescription.MipmapFilter = reference.MipmapFilter;

    return imageDescription;
}

ImageSubresourceDescription Graph::GetImageSubresourceDescription(ImageResource image) const
{
    return image.m_Extra == ImageResource::NO_EXTRA ?
        ImageSubresourceDescription{} :
        m_Images[image.m_Index].Description.AdditionalViews[image.m_Extra];
}

ImageLayout& Graph::GetImageLayout(ImageResource image)
{
    return image.m_Extra == ImageResource::NO_EXTRA ?
        m_Images[image.m_Index].Layout :
        m_Images[image.m_Index].Extras[image.m_Extra].Layout;
}

BufferResource Graph::AddBufferAccess(BufferResource resource, AccessType type, RGBuffer&, PipelineStage stage,
    PipelineAccess access)
{
    if (type != AccessType::Read)
        resource.m_Version++;
    m_BufferAccesses.push_back({
        .Info = ResourceAccessInfo{.PassIndex = CurrentPassIndex(), .Type = type, .Stage = stage, .Access = access},
        .Resource = resource
    });

    return resource;
}

ImageResource Graph::AddImageAccess(ImageResource resource, AccessType type, RGImage& image, PipelineStage stage,
    PipelineAccess access)
{
    RG_CHECK_RETURN(resource.HasFlags(ResourceFlags::Split) || type == AccessType::Split ||
        image.State == RGImageState::Merged,
        "Cannot use primary image while it is not merged")

    if (resource.HasFlags(ResourceFlags::Split) && type != AccessType::Merge)
        image.State = RGImageState::Split;

    if (resource.HasFlags(ResourceFlags::AutoUpdate))
        resource.m_Version = resource.m_Extra == ImageResource::NO_EXTRA ?
            image.LatestVersion :
            image.Extras[resource.m_Extra].Version;
    if (enumHasAny(type, AccessType::Write | AccessType::Merge | AccessType::Split))
    {
        resource.m_Version++;
        image.LatestVersion = std::max(image.LatestVersion, resource.m_Version);
        if (resource.m_Extra != ImageResource::NO_EXTRA)
            image.Extras[resource.m_Extra].Version =
                std::max(image.Extras[resource.m_Extra].Version, resource.m_Version);
    }
    m_ImageAccesses.push_back({
        .Info = ResourceAccessInfo{.PassIndex = CurrentPassIndex(), .Type = type, .Stage = stage, .Access = access},
        .Resource = resource
    });

    return resource;
}

u32 Graph::CurrentPassIndex() const
{
    return m_PassIndicesStack.back();
}

Pass& Graph::CurrentPass() const
{
    return *m_Passes[CurrentPassIndex()];
}

bool Graph::IsPassSplitOrMerge(const Pass& pass) const
{
    if (pass.Name() != m_MergePassName && pass.Name() != m_SplitPassName)
        return false;
    
    ASSERT(
        pass.m_BarriersToWait.empty() && 
        pass.m_SplitBarriersToWait.empty() && 
        pass.m_SplitBarriersToSignal.empty() &&
        !m_ResourceUploader.HasUploads(pass)
    )
    
    return true;
}
}

#undef RG_CHECK_RETURN
#undef RG_CHECK_RETURN_VOID
