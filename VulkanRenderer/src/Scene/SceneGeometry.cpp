#include "rendererpch.h"

#include "SceneGeometry.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "RenderGraph/Passes/Generated/Types/SkinnedVertexUniform.generated.h"

namespace
{
void growBufferArena(BufferArena arena, u64 newMinSize, RenderCommandList& cmdList)
{
    static constexpr f32 GROWTH_RATE = 1.5f;

    const u64 newSize = std::max(newMinSize, (u64)GROWTH_RATE * Device::GetBufferArenaSizeBytesPhysical(arena));
    Device::ResizeBufferArenaPhysical(arena, newSize, cmdList);
}

BufferSuballocation suballocateResizeIfFailed(BufferArena arena, u64 sizeBytes, u32 alignment,
    RenderCommandList& cmdList)
{
    BufferSuballocationResult suballocationResult = Device::BufferArenaSuballocate(arena, sizeBytes, alignment);
    if (suballocationResult.has_value())
        return suballocationResult.value();

    ASSERT(suballocationResult.error() != BufferSuballocationError::OutOfVirtualMemory,
        "Out of virtual memory for buffer arena")

    growBufferArena(arena, Device::GetBufferArenaSizeBytesPhysical(arena) + sizeBytes, cmdList);
    suballocationResult = Device::BufferArenaSuballocate(arena, sizeBytes, alignment);
    ASSERT(suballocationResult.has_value(), "Failed to suballocate")

    return suballocationResult.value();
}

template <typename T>
void writeSuballocation(BufferArena arena, const std::vector<T>& data,
    SceneGeometry::SceneInfoOffsetType bufferType, SceneGeometry::SceneInfoOffsets& offsets, FrameContext& ctx)
{
    if (data.empty())
        return;
    const BufferSuballocation suballocation = suballocateResizeIfFailed(arena,
        data.size() * sizeof(T), sizeof(T), ctx.CommandList);
    offsets.ElementOffsets[(u32)bufferType] = (u32)(suballocation.Description.Offset / sizeof(T));
    offsets.Suballocations[(u32)bufferType] = suballocation.Handle;
    ctx.ResourceUploader->UpdateBuffer(suballocation.Buffer, data, suballocation.Description.Offset);
}
}

SceneGeometry SceneGeometry::CreateEmpty(DeletionQueue& deletionQueue)
{
    SceneGeometry geometry = {};
    geometry.Attributes = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.Indices = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.Meshlets = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_MESHLETS_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.RenderObjects = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.JointMatrices = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_JOINT_MATRICES_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.Skins = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_SKINS_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);
    geometry.Materials = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_MATERIALS_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);

    geometry.RenderObjectSkinnedInfos = Device::CreateBufferArena({
            .Buffer = Device::CreateBuffer({
                .Description = {
                    .SizeBytes = DEFAULT_SKINNED_RENDER_OBJECTS_BUFFER_ARENA_SIZE_BYTES,
                    .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
                },
            }, deletionQueue),
            .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES
        },
        deletionQueue);

    return geometry;
}

void SceneGeometry::Add(const lux::SceneAsset& scene, FrameContext& ctx)
{
    using enum SceneInfoOffsetType;

    auto& geometry = scene.Geometry;

    SceneInfoOffsets sceneInfoOffsets = {};
    writeSuballocation(Attributes, geometry.Positions, Position, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, geometry.Normals, Normal, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, geometry.Tangents, Tangent, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, geometry.UVs, Uv, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, geometry.Joints, Joint, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, geometry.Weights, Weight, sceneInfoOffsets, ctx);
    writeSuballocation(Indices, geometry.Indices, Index, sceneInfoOffsets, ctx);

    std::vector<MeshletBoundsGPU> meshletBounds;
    std::vector<MeshletGPU> meshlets;
    meshletBounds.reserve(geometry.Meshlets.size());
    meshlets.reserve(geometry.Meshlets.size());
    for (auto& meshlet : geometry.Meshlets)
    {
        meshletBounds.push_back({
            {
                .ConeX = meshlet.Cone.AxisX,
                .ConeY = meshlet.Cone.AxisY,
                .ConeZ = meshlet.Cone.AxisZ,
                .ConeCutoff = meshlet.Cone.Cutoff,
                .X = meshlet.Sphere.Center.x,
                .Y = meshlet.Sphere.Center.y,
                .Z = meshlet.Sphere.Center.z,
                .R = meshlet.Sphere.Radius,
            }
        });
        meshlets.push_back({
            {
                .FirstIndex = meshlet.FirstIndex,
                .IndexCount = meshlet.IndexCount,
                .FirstVertex = meshlet.FirstVertex,
                .VertexCount = meshlet.VertexCount,
            }
        });
    }
    writeSuballocation(this->Meshlets, meshletBounds, MeshletBounds, sceneInfoOffsets, ctx);
    writeSuballocation(this->Meshlets, meshlets, Meshlets, sceneInfoOffsets, ctx);

    writeSuballocation(this->Materials, geometry.Materials, Materials, sceneInfoOffsets, ctx);

    MaterialsCpu.reserve(MaterialsCpu.size() + (u32)geometry.MaterialsCpu.size());
    for (auto& material : geometry.MaterialsCpu)
        MaterialsCpu.push_back(material.Handle);

    m_SceneInfoOffsets[&scene] = sceneInfoOffsets;
}

SceneGeometry::AddInstanceResult SceneGeometry::AddInstance(const lux::SceneAsset& scene,
    lux::SceneInstanceHandle instance, FrameContext& ctx)
{
    auto& geometry = scene.Geometry;

    if (geometry.RenderObjects.empty())
        return {};

    const SceneInfoOffsets& sceneInfoOffsets = m_SceneInfoOffsets[&scene];

    const u64 renderObjectsSizeBytes = geometry.RenderObjects.size() * sizeof(RenderObjectGPU);
    const u64 renderObjectSkinnedInfosSizeBytes = geometry.SkinnedRenderObjects.size() * sizeof(
        RenderObjectSkinnedInfoGPU);
    const u32 jointCount = std::ranges::fold_left(
        geometry.Skins, 0u, [](u32 sum, auto& skin) -> u32 { return sum + (u32)skin.JointNodes.size(); });
    u32 skinnedVertexCount{};
    u32 skinnedMeshletCount{};
    for (auto& renderObject : geometry.RenderObjects)
    {
        if (renderObject.SkinnedRenderObjectIndex == lux::SceneRenderObject::INVALID)
            continue;
        
        skinnedVertexCount += renderObject.VertexCount;
        skinnedMeshletCount += renderObject.MeshletCount;
    }
    const u64 jointMatricesSizeBytes = jointCount * sizeof(glm::mat4);
    const u64 skinnedVerticesSizeBytes = skinnedVertexCount * sizeof(SkinnedVertexGPU);
    const u64 skinnedMeshletBoundsSizeBytes = skinnedMeshletCount * sizeof(MeshletBoundsGPU);
    const u64 skinsSizeBytes = geometry.SkinnedRenderObjects.size() * sizeof(SkinGPU);
    const bool hasSkinInfo = skinsSizeBytes > 0;

    SceneInstanceInfo instanceInfo = {};
    instanceInfo.RenderObjectsSuballocation = suballocateResizeIfFailed(RenderObjects,
        renderObjectsSizeBytes, 0, ctx.CommandList);
    const u32 firstRenderObject =
        (u32)(instanceInfo.RenderObjectsSuballocation.Description.Offset / sizeof(RenderObjectGPU));

    u32 currentSkinOffset = 0;
    u32 currentJointMatrixOffset = 0;
    u32 currentSkinnedVertexOffset = 0;
    u32 currentSkinnedMeshletBoundOffset = 0;
    if (hasSkinInfo)
    {
        instanceInfo.RenderObjectSkinnedInfosSuballocation = suballocateResizeIfFailed(RenderObjectSkinnedInfos,
            renderObjectSkinnedInfosSizeBytes, 0, ctx.CommandList);
        instanceInfo.JointMatricesSuballocation = suballocateResizeIfFailed(JointMatrices,
            jointMatricesSizeBytes, 0, ctx.CommandList);
        instanceInfo.SkinsSuballocation = suballocateResizeIfFailed(Skins, skinsSizeBytes, 0, ctx.CommandList);
        instanceInfo.SkinnedVertexSuballocation = suballocateResizeIfFailed(Attributes, skinnedVerticesSizeBytes, 
            sizeof(SkinnedVertexGPU), ctx.CommandList);
        instanceInfo.SkinnedMeshletBoundSuballocation = suballocateResizeIfFailed(Meshlets,
            skinnedMeshletBoundsSizeBytes, sizeof(MeshletBoundsGPU), ctx.CommandList);
        currentSkinOffset = (u32)(instanceInfo.SkinsSuballocation.Description.Offset / sizeof(SkinGPU));
        currentJointMatrixOffset =
            (u32)(instanceInfo.JointMatricesSuballocation.Description.Offset / sizeof(glm::mat4));
        currentSkinnedVertexOffset = 
            (u32)(instanceInfo.SkinnedVertexSuballocation.Description.Offset / sizeof(SkinnedVertexGPU));
        currentSkinnedMeshletBoundOffset =
            (u32)(instanceInfo.SkinnedMeshletBoundSuballocation.Description.Offset / sizeof(MeshletBoundsGPU));
    }

    auto* renderObjects = ctx.ResourceUploader->MapBuffer<RenderObjectGPU>({
        .Buffer = Device::GetBufferArenaUnderlyingBuffer(RenderObjects),
        .Description = {
            .SizeBytes = renderObjectsSizeBytes,
            .Offset = instanceInfo.RenderObjectsSuballocation.Description.Offset
        }
    });
    auto* renderObjectSkinnedInfos = ctx.ResourceUploader->MapBuffer<RenderObjectSkinnedInfoGPU>({
        .Buffer = Device::GetBufferArenaUnderlyingBuffer(RenderObjectSkinnedInfos),
        .Description = {
            .SizeBytes = renderObjectsSizeBytes,
            .Offset = instanceInfo.RenderObjectSkinnedInfosSuballocation.Description.Offset
        }
    });

    u32 skinnedInfoIndex = 0;
    for (auto&& [renderObjectIndex, renderObject] : std::views::enumerate(geometry.RenderObjects))
    {
        using enum SceneInfoOffsetType;

        const u32 renderObjectFirstIndex = renderObject.FirstIndex;
        const u32 renderObjectFirstVertex = renderObject.FirstVertex;
        const u32 renderObjectMaterial = renderObject.Material;
        const bool hasSkin = renderObject.SkinnedRenderObjectIndex != lux::SceneRenderObject::INVALID;

        renderObjects[renderObjectIndex] = {
            {
                /* this value is irrelevant, because the transform will be set by SceneHierarchy */
                .Transform = glm::mat4(1.0f),
                .BoundingSphere = glm::vec4(renderObject.BoundingSphere.Center, renderObject.BoundingSphere.Radius),
                .MaterialId = sceneInfoOffsets.ElementOffsets[(u32)Materials] + renderObjectMaterial,
                .PositionIndex = sceneInfoOffsets.ElementOffsets[(u32)Position] + renderObjectFirstVertex,
                .NormalIndex = sceneInfoOffsets.ElementOffsets[(u32)Normal] + renderObjectFirstVertex,
                .TangentIndex = sceneInfoOffsets.ElementOffsets[(u32)Tangent] + renderObjectFirstVertex,
                .UvIndex = sceneInfoOffsets.ElementOffsets[(u32)Uv] + renderObjectFirstVertex,
                .IndexIndex = sceneInfoOffsets.ElementOffsets[(u32)Index] + renderObjectFirstIndex,
                .MeshletIndex = sceneInfoOffsets.ElementOffsets[(u32)Meshlets] + renderObject.FirstMeshlet,
                .MeshletBoundIndex = sceneInfoOffsets.ElementOffsets[(u32)MeshletBounds] + renderObject.FirstMeshlet,
                .MeshletCount = renderObject.MeshletCount,
                .SkinIndex = hasSkin ? 
                    renderObject.SkinnedRenderObjectIndex + currentSkinOffset : lux::SceneRenderObject::INVALID,
            }
        };

        if (hasSkin)
        {
            renderObjectSkinnedInfos[skinnedInfoIndex++] = {
                {
                    .RenderObjectIndex = firstRenderObject + (u32)renderObjectIndex,
                    .MeshletBoundIndex = renderObjects[renderObjectIndex].MeshletBoundIndex,
                    .PositionIndex = renderObjects[renderObjectIndex].PositionIndex,
                    .NormalIndex = renderObjects[renderObjectIndex].NormalIndex,
                    .TangentIndex = renderObjects[renderObjectIndex].TangentIndex,
                    .SkinnedVertexIndex = currentSkinnedVertexOffset,
                    .SkinnedMeshletBoundIndex = currentSkinnedMeshletBoundOffset,
                    .VertexCount = renderObject.VertexCount
                }
            };
            currentSkinnedVertexOffset += renderObject.VertexCount;
            currentSkinnedMeshletBoundOffset += renderObject.MeshletCount;
        }
    }

    if (hasSkinInfo)
    {
        SkinGPU* skins = ctx.ResourceUploader->MapBuffer<SkinGPU>({
            .Buffer = Device::GetBufferArenaUnderlyingBuffer(Skins),
            .Description = {
                .SizeBytes = skinsSizeBytes,
                .Offset = instanceInfo.SkinsSuballocation.Description.Offset
            }
        });

        for (auto&& [skinIndex, skin] : std::views::enumerate(geometry.SkinnedRenderObjects))
            skins[skinIndex] = SkinGPU{
                {
                    .JointMatrixIndex = skin.FirstJointMatrix + currentJointMatrixOffset,
                    .JointIndex = skin.FirstJoint + sceneInfoOffsets.ElementOffsets[(u32)SceneInfoOffsetType::Joint],
                    .JointOffset = sceneInfoOffsets.ElementOffsets[(u32)SceneInfoOffsetType::Joint],
                    .WeightIndex = skin.FirstWeight + sceneInfoOffsets.ElementOffsets[(u32)SceneInfoOffsetType::Weight],
                }
            };

        instanceInfo.SkinnedRenderObjectCount = (u32)geometry.SkinnedRenderObjects.size();
        instanceInfo.SkinnedMeshletCount = skinnedMeshletCount;
        instanceInfo.SkinnedVertexCount = skinnedVertexCount;

        SkinnedRenderObjectCount += instanceInfo.SkinnedRenderObjectCount;
        SkinnedMeshletCount += instanceInfo.SkinnedMeshletCount;
        SkinnedVertexCount += instanceInfo.SkinnedVertexCount;
    }

    m_InstancesInfo.emplace(instance, instanceInfo);

    return {
        .FirstRenderObject = firstRenderObject,
        .FirstJoint = sceneInfoOffsets.ElementOffsets[(u32)SceneInfoOffsetType::Joint],
        .FirstJointMatrix = currentJointMatrixOffset
    };
}

void SceneGeometry::UpdateMaterials(const lux::SceneAsset& scene, FrameContext& ctx)
{
    using enum SceneInfoOffsetType;

    auto it = m_SceneInfoOffsets.find(&scene);
    if (it == m_SceneInfoOffsets.end())
        return;
    const auto& suballocations = it->second.Suballocations;
    Device::BufferArenaFree(this->Materials, suballocations[(u32)Materials]);

    writeSuballocation(this->Materials, scene.Geometry.Materials, Materials, it->second, ctx);
}

void SceneGeometry::Delete(const lux::SceneAsset& scene)
{
    using enum SceneInfoOffsetType;

    auto it = m_SceneInfoOffsets.find(&scene);
    if (it == m_SceneInfoOffsets.end())
        return;

    const auto& suballocations = it->second.Suballocations;

    Device::BufferArenaFree(Attributes, suballocations[(u32)Position]);
    Device::BufferArenaFree(Attributes, suballocations[(u32)Normal]);
    Device::BufferArenaFree(Attributes, suballocations[(u32)Tangent]);
    Device::BufferArenaFree(Attributes, suballocations[(u32)Uv]);
    Device::BufferArenaFree(Indices, suballocations[(u32)Index]);
    Device::BufferArenaFree(this->Meshlets, suballocations[(u32)Meshlets]);
    Device::BufferArenaFree(this->Meshlets, suballocations[(u32)MeshletBounds]);
    Device::BufferArenaFree(this->Materials, suballocations[(u32)Materials]);

    m_SceneInfoOffsets.erase(it);
}

void SceneGeometry::DeleteRenderObjects(lux::SceneInstanceHandle instance)
{
    auto& instanceInfo = m_InstancesInfo.at(instance);
    Device::BufferArenaFree(RenderObjects, instanceInfo.RenderObjectsSuballocation.Handle);

    if (instanceInfo.SkinnedRenderObjectCount > 0)
    {
        Device::BufferArenaFree(RenderObjectSkinnedInfos, instanceInfo.RenderObjectSkinnedInfosSuballocation.Handle);
        Device::BufferArenaFree(JointMatrices, instanceInfo.JointMatricesSuballocation.Handle);
        Device::BufferArenaFree(Skins, instanceInfo.SkinsSuballocation.Handle);
        Device::BufferArenaFree(Attributes, instanceInfo.SkinnedVertexSuballocation.Handle);
        Device::BufferArenaFree(Meshlets, instanceInfo.SkinnedMeshletBoundSuballocation.Handle);
        SkinnedRenderObjectCount -= instanceInfo.SkinnedRenderObjectCount;
        SkinnedMeshletCount -= instanceInfo.SkinnedMeshletCount;
        SkinnedVertexCount -= instanceInfo.SkinnedVertexCount;
    }

    m_InstancesInfo.erase(instance);
}
