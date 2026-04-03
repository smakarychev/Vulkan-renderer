#include "rendererpch.h"

#include "SceneGeometry.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Rendering/Image/ImageUtility.h"

#include <AssetLib/Materials/MaterialAsset.h>

namespace
{
void growBufferArena(BufferArena arena, u64 newMinSize, RenderCommandList& cmdList)
{
    static constexpr f32 GROWTH_RATE = 1.5;

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
    geometry.RenderObjects.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
    geometry.Materials.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = DEFAULT_MATERIALS_BUFFER_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);

    return geometry;
}

void SceneGeometry::Add(SceneInstance instance, FrameContext& ctx)
{
    using enum SceneInfoOffsetType;

    auto& sceneInfo = *instance.m_SceneInfo;

    SceneInfoOffsets sceneInfoOffsets = {};
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Positions, Position, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Normals, Normal, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Tangents, Tangent, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.UVs, Uv, sceneInfoOffsets, ctx);
    writeSuballocation(Indices, sceneInfo.m_Geometry.Indices, Index, sceneInfoOffsets, ctx);

    std::vector<MeshletBoundsGPU> meshletBounds;
    std::vector<MeshletGPU> meshlets;
    meshletBounds.reserve(sceneInfo.m_Geometry.Meshlets.size());
    meshlets.reserve(sceneInfo.m_Geometry.Meshlets.size());
    for (auto& meshlet : sceneInfo.m_Geometry.Meshlets)
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

    sceneInfoOffsets.MaterialOffset = (u32)(Materials.Offset / sizeof(MaterialGPU));
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(Materials,
        sceneInfo.m_Geometry.Materials, ctx.CommandList, *ctx.ResourceUploader);
    MaterialsCpu.reserve(MaterialsCpu.size() + (u32)sceneInfo.m_Geometry.MaterialsCpu.size());
    for (auto& material : sceneInfo.m_Geometry.MaterialsCpu)
        MaterialsCpu.push_back(material);

    m_SceneInfoOffsets[&sceneInfo] = sceneInfoOffsets;
}

SceneGeometry::AddRenderObjectsResult SceneGeometry::AddRenderObjects(SceneInstance instance, FrameContext& ctx)
{
    auto& sceneInfo = *instance.m_SceneInfo;
    const SceneInfoOffsets& sceneInfoOffsets = m_SceneInfoOffsets[&sceneInfo];

    const u64 renderObjectsSizeBytes = sceneInfo.m_Geometry.RenderObjects.size() * sizeof(RenderObjectGPU);
    PushBuffers::grow<BufferAsymptoticGrowthPolicy>(RenderObjects, renderObjectsSizeBytes, ctx.CommandList);

    RenderObjectGPU* renderObjects = ctx.ResourceUploader->MapBuffer<RenderObjectGPU>({
        .Buffer = RenderObjects.Buffer,
        .Description = {
            .SizeBytes = renderObjectsSizeBytes,
            .Offset = RenderObjects.Offset
        }
    });

    const u32 currentRenderObjectIndex = (u32)(RenderObjects.Offset / sizeof(RenderObjectGPU));
    for (auto&& [renderObjectIndex, renderObject] : std::views::enumerate(sceneInfo.m_Geometry.RenderObjects))
    {
        using enum SceneInfoOffsetType;
        
        const u32 renderObjectFirstIndex = renderObject.FirstIndex;
        const u32 renderObjectFirstVertex = renderObject.FirstVertex;
        const u32 renderObjectMaterial = renderObject.Material + sceneInfoOffsets.MaterialOffset;

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

    RenderObjects.Offset += renderObjectsSizeBytes;

    return {
        .FirstRenderObject = currentRenderObjectIndex,
    };
}
