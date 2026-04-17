#include "rendererpch.h"

#include "SceneGeometry.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"

#include <AssetLib/Materials/MaterialAsset.h>

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
    writeSuballocation(Indices, geometry.Indices, Index, sceneInfoOffsets, ctx);

    std::vector<MeshletBoundsGPU> meshletBounds;
    std::vector<MeshletGPU> meshlets;
    meshletBounds.reserve(geometry.Meshlets.size());
    meshlets.reserve(geometry.Meshlets.size());
    for (auto& meshlet : geometry.Meshlets)
    {
        meshletBounds.push_back({{
            .ConeX = meshlet.Cone.AxisX,
            .ConeY = meshlet.Cone.AxisY,
            .ConeZ = meshlet.Cone.AxisZ,
            .ConeCutoff = meshlet.Cone.Cutoff,
            .X = meshlet.Sphere.Center.x,
            .Y = meshlet.Sphere.Center.y,
            .Z = meshlet.Sphere.Center.z,
            .R = meshlet.Sphere.Radius,
        }});
        meshlets.push_back({{
            .FirstIndex = meshlet.FirstIndex,
            .IndexCount = meshlet.IndexCount,
            .FirstVertex = meshlet.FirstVertex,
            .VertexCount = meshlet.VertexCount,
        }});
    }
    writeSuballocation(this->Meshlets, meshletBounds, MeshletBounds, sceneInfoOffsets, ctx);
    writeSuballocation(this->Meshlets, meshlets, Meshlets, sceneInfoOffsets, ctx);

    writeSuballocation(this->Materials, geometry.Materials, Materials, sceneInfoOffsets, ctx);
    
    MaterialsCpu.reserve(MaterialsCpu.size() + (u32)geometry.MaterialsCpu.size());
    for (auto& material : geometry.MaterialsCpu)
        MaterialsCpu.push_back(material.Handle);

    m_SceneInfoOffsets[&scene] = sceneInfoOffsets;
}

SceneGeometry::AddRenderObjectsResult SceneGeometry::AddRenderObjects(const lux::SceneAsset& scene,
    lux::SceneInstanceHandle instance, FrameContext& ctx)
{
    auto& geometry = scene.Geometry;
    
    if (geometry.RenderObjects.empty())
        return {.FirstRenderObject = 0};
        
    const SceneInfoOffsets& sceneInfoOffsets = m_SceneInfoOffsets[&scene];

    const u64 renderObjectsSizeBytes = geometry.RenderObjects.size() * sizeof(RenderObjectGPU);

    SceneInstanceInfo instanceInfo = {};
    instanceInfo.RenderObjectsSuballocation = suballocateResizeIfFailed(RenderObjects,
        renderObjectsSizeBytes, 0, ctx.CommandList);
    m_InstancesInfo.emplace(instance, instanceInfo);
    
    RenderObjectGPU* renderObjects = ctx.ResourceUploader->MapBuffer<RenderObjectGPU>({
        .Buffer = Device::GetBufferArenaUnderlyingBuffer(RenderObjects),
        .Description = {
            .SizeBytes = renderObjectsSizeBytes,
            .Offset = instanceInfo.RenderObjectsSuballocation.Description.Offset
        }
    });

    for (auto&& [renderObjectIndex, renderObject] : std::views::enumerate(geometry.RenderObjects))
    {
        using enum SceneInfoOffsetType;
        
        const u32 renderObjectFirstIndex = renderObject.FirstIndex;
        const u32 renderObjectFirstVertex = renderObject.FirstVertex;
        const u32 renderObjectMaterial = renderObject.Material + sceneInfoOffsets.ElementOffsets[(u32)Materials];

        renderObjects[renderObjectIndex] = {
            {
                /* this value is irrelevant, because the transform will be set by SceneHierarchy */
                .Transform = glm::mat4(1.0f),
                .BoundingSphere = glm::vec4(renderObject.BoundingSphere.Center, renderObject.BoundingSphere.Radius),
                .MaterialId = renderObjectMaterial,
                .PositionIndex = sceneInfoOffsets.ElementOffsets[(u32)Position] + renderObjectFirstVertex,
                .NormalIndex = sceneInfoOffsets.ElementOffsets[(u32)Normal] + renderObjectFirstVertex,
                .TangentIndex = sceneInfoOffsets.ElementOffsets[(u32)Tangent] + renderObjectFirstVertex,
                .UvIndex = sceneInfoOffsets.ElementOffsets[(u32)Uv] + renderObjectFirstVertex,
                .IndexIndex = sceneInfoOffsets.ElementOffsets[(u32)Index] + renderObjectFirstIndex,
                .MeshletIndex = sceneInfoOffsets.ElementOffsets[(u32)Meshlets] + renderObject.FirstMeshlet,
                .MeshletBoundsIndex = sceneInfoOffsets.ElementOffsets[(u32)MeshletBounds] + renderObject.FirstMeshlet,
                .MeshletCount = renderObject.MeshletCount,
            }
        };
    }

    return {
        .FirstRenderObject =
            (u32)(instanceInfo.RenderObjectsSuballocation.Description.Offset / sizeof(RenderObjectGPU)),
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
    m_InstancesInfo.erase(instance);
}
