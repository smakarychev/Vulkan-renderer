#include "rendererpch.h"

#include "SceneGeometry.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Rendering/Image/ImageUtility.h"
#include "v2/Materials/MaterialAsset.h"

namespace
{
template <typename T>
void copyToVector(std::vector<T>& vec, std::byte* data, u64 sizeBytes)
{
    ASSERT(sizeBytes % sizeof(T) == 0, "Data size in bytes is not a multiple of element size")
    ASSERT((u64)data % alignof(T) == 0, "Data is not aligned properly")

    vec.resize(sizeBytes / sizeof(T));
    memcpy(vec.data(), data, sizeBytes);
}

void loadBuffers(SceneGeometryInfo& geometry, lux::assetlib::SceneAsset& scene)
{
    using enum lux::assetlib::SceneAssetBufferViewType;
    ASSERT(scene.Header.Buffers.size() == 1, "Multiple sub-scenes are not supported")
    auto& sceneBuffer = scene.BuffersData[0];
    std::byte* bufferData = sceneBuffer.data();
    auto& views = scene.Header.BufferViews;

    copyToVector(geometry.Indices,
        bufferData + views[(u32)Index].OffsetBytes, views[(u32)Index].LengthBytes);
    copyToVector(geometry.Positions,
        bufferData + views[(u32)Position].OffsetBytes, views[(u32)Position].LengthBytes);
    copyToVector(geometry.Normals,
        bufferData + views[(u32)Normal].OffsetBytes, views[(u32)Normal].LengthBytes);
    copyToVector(geometry.Tangents,
        bufferData + views[(u32)Tangent].OffsetBytes, views[(u32)Tangent].LengthBytes);
    copyToVector(geometry.UVs,
        bufferData + views[(u32)Uv].OffsetBytes, views[(u32)Uv].LengthBytes);
    copyToVector(geometry.Meshlets,
        bufferData + views[(u32)Meshlet].OffsetBytes, views[(u32)Meshlet].LengthBytes);
}

MaterialFlags materialToMaterialFlags(const lux::MaterialAsset& material)
{
    MaterialFlags materialFlags = MaterialFlags::None;
    if (material.AlphaMode == lux::MaterialAlphaMode::Opaque)
        materialFlags |= MaterialFlags::Opaque;
    else if (material.AlphaMode == lux::MaterialAlphaMode::Mask)
        materialFlags |= MaterialFlags::AlphaMask;
    else if (material.AlphaMode == lux::MaterialAlphaMode::Translucent)
        materialFlags |= MaterialFlags::Translucent;

    if (material.DoubleSided)
        materialFlags |= MaterialFlags::TwoSided;

    return materialFlags;
}

void loadMaterials(SceneGeometryInfo& geometry, lux::assetlib::SceneAsset& scene,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue,
    lux::AssetSystem& assetSystem,
    lux::ImageAssetManager& imageAssetManager,
    lux::MaterialAssetManager& materialAssetManager)
{
    static constexpr TextureHandle INVALID_TEXTURE = {~0lu};
    std::unordered_map<lux::ImageAsset, TextureHandle> loadedTextures;

    auto get = [&](lux::ImageHandle image) -> Image{
        return image.IsValid() ? imageAssetManager.Get(image) : Image{};
    };

    auto processTexture = [&](const lux::assetlib::SceneAssetTextureSample& sample,
        lux::ImageAsset image, TextureHandle fallback) -> TextureHandle
    {
        if (!image.HasValue())
            return fallback;
        if (sample.UvIndex > 0)
        {
            LUX_LOG_WARN("Skipping texture {}, as it uses uv set other than 0", sample.UvIndex);
            return fallback;
        }
        
        if (!loadedTextures.contains(image))
            loadedTextures[image] = texturesRingBuffer.AddTexture(image);

        return loadedTextures[image];
    };
    geometry.Materials.reserve(scene.Header.Materials.size());
    geometry.MaterialsCpu.reserve(scene.Header.Materials.size());
    for (auto& material : scene.Header.Materials)
    {
        auto* materialAssetInfo = assetSystem.Resolve(material.MaterialAsset);
        if (!materialAssetInfo)
            continue;
        
        auto materialHandle = materialAssetManager.LoadResource({.Path = materialAssetInfo->Path});
        if (!materialHandle.IsValid())
            continue;

        auto* materialAsset = materialAssetManager.Get(materialHandle);
        if (!materialAsset)
            continue;
        
        geometry.Materials.push_back({
            {
                .Albedo = materialAsset->BaseColor,
                .Metallic = materialAsset->Metallic,
                .Roughness = materialAsset->Roughness,
                .AlbedoTexture = processTexture(
                // Todo: images are already resolved!! 
                    material.BaseColorSample, get(materialAsset->BaseColorTexture),
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White)),
                .NormalTexture = processTexture(
                    material.NormalSample, get(materialAsset->NormalTexture),
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::NormalMap)),
                .MetallicRoughnessTexture = processTexture(
                    material.MetallicRoughnessSample, get(materialAsset->MetallicRoughnessTexture),
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White)),
                .AmbientOcclusionTexture = processTexture(
                    material.OcclusionSample, get(materialAsset->OcclusionTexture),
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White)),
                .EmissiveTexture = processTexture(
                    material.EmissiveSample, get(materialAsset->EmissiveTexture),
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::Black))
            }
        });

        geometry.MaterialsCpu.push_back({
            .Flags = materialToMaterialFlags(*materialAsset)
        });
    }
}

void loadRenderObjects(SceneGeometryInfo& geometry, lux::assetlib::SceneAsset& scene)
{
    geometry.RenderObjects.reserve(scene.Header.Meshes.size());
    for (auto& meshInfo : scene.Header.Meshes)
    {
        ASSERT(meshInfo.Primitives.size() < 2, "Render objects with more that 1 primitives are not supported")
        for (auto& primitive : meshInfo.Primitives)
        {
            const u32 firstIndex = (u32)(scene.Header.Accessors[primitive.IndicesAccessor].OffsetBytes /
                sizeof(lux::assetlib::SceneAssetIndexType));
            const u32 firstVertex = (u32)(
                scene.Header.Accessors[primitive.FindAttribute(
                    lux::assetlib::SceneAssetPrimitive::ATTRIBUTE_POSITION_NAME)->Accessor].OffsetBytes /
                sizeof(glm::vec3));
            const u32 firstMeshlet = (u32)(
                scene.Header.Accessors[primitive.FindAttribute(
                    lux::assetlib::SceneAssetPrimitive::ATTRIBUTE_MESHLET_NAME)->Accessor].OffsetBytes /
                sizeof(lux::assetlib::SceneAssetMeshlet));
            const u32 meshletsCount = scene.Header.Accessors[primitive.FindAttribute(
                lux::assetlib::SceneAssetPrimitive::ATTRIBUTE_MESHLET_NAME)->Accessor].Count;

            geometry.RenderObjects.push_back({
                .Material = primitive.Material,
                .FirstIndex = firstIndex,
                .FirstVertex = firstVertex,
                .FirstMeshlet = firstMeshlet,
                .MeshletCount = meshletsCount,
                .BoundingBox = primitive.BoundingBox,
                .BoundingSphere = primitive.BoundingSphere
            });
        }
    }
}

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
    lux::assetlib::SceneAssetBufferViewType bufferType, SceneGeometry::SceneInfoOffsets& offsets, FrameContext& ctx)
{
    static constexpr u32 ALIGNMENT = 1;
    if (data.empty())
        return;
    const BufferSuballocation suballocation = suballocateResizeIfFailed(arena,
        data.size() * sizeof(T), ALIGNMENT, ctx.CommandList);
    offsets.ElementOffsets[(u32)bufferType] = (u32)(suballocation.Description.Offset / sizeof(T));
    ctx.ResourceUploader->UpdateBuffer(suballocation.Buffer, data, suballocation.Description.Offset);
}
}

SceneGeometryInfo SceneGeometryInfo::FromAsset(lux::assetlib::SceneAsset& scene,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue,
    lux::AssetSystem& assetSystem,
    lux::ImageAssetManager& imageAssetManager,
    lux::MaterialAssetManager& materialAssetManager)
{
    SceneGeometryInfo geometryInfo = {};
    loadBuffers(geometryInfo, scene);
    loadMaterials(geometryInfo, scene, texturesRingBuffer, deletionQueue, assetSystem, imageAssetManager, materialAssetManager);
    loadRenderObjects(geometryInfo, scene);

    return geometryInfo;
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
    geometry.RenderObjects.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
    geometry.Meshlets.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = DEFAULT_MESHLET_BUFFER_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
    geometry.Commands.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = DEFAULT_COMMANDS_BUFFER_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Source
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
    using enum lux::assetlib::SceneAssetBufferViewType;

    auto& sceneInfo = *instance.m_SceneInfo;

    SceneInfoOffsets sceneInfoOffsets = {};
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Positions, Position, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Normals, Normal, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Tangents, Tangent, sceneInfoOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.UVs, Uv, sceneInfoOffsets, ctx);
    writeSuballocation(Indices, sceneInfo.m_Geometry.Indices, Index, sceneInfoOffsets, ctx);

    sceneInfoOffsets.MaterialOffset = (u32)(Materials.Offset / sizeof(MaterialGPU));
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(Materials,
        sceneInfo.m_Geometry.Materials, ctx.CommandList, *ctx.ResourceUploader);
    MaterialsCpu.reserve(MaterialsCpu.size() + (u32)sceneInfo.m_Geometry.MaterialsCpu.size());
    for (auto& material : sceneInfo.m_Geometry.MaterialsCpu)
        MaterialsCpu.push_back(material);

    m_SceneInfoOffsets[&sceneInfo] = sceneInfoOffsets;
}

SceneGeometry::AddCommandsResult SceneGeometry::AddCommands(SceneInstance instance, FrameContext& ctx)
{
    using enum lux::assetlib::SceneAssetBufferViewType;

    auto& sceneInfo = *instance.m_SceneInfo;
    const SceneInfoOffsets& sceneInfoOffsets = m_SceneInfoOffsets[&sceneInfo];

    const u64 renderObjectsSizeBytes = sceneInfo.m_Geometry.RenderObjects.size() * sizeof(RenderObjectGPU);
    const u32 meshletCount = (u32)sceneInfo.m_Geometry.Meshlets.size();
    const u64 commandsSizeBytes = meshletCount * sizeof(IndirectDrawCommand);
    const u64 meshletsSizeBytes = meshletCount * sizeof(MeshletGPU);
    PushBuffers::grow<BufferAsymptoticGrowthPolicy>(RenderObjects, renderObjectsSizeBytes, ctx.CommandList);
    PushBuffers::grow<BufferAsymptoticGrowthPolicy>(Commands, commandsSizeBytes, ctx.CommandList);
    PushBuffers::grow<BufferAsymptoticGrowthPolicy>(Meshlets, meshletsSizeBytes, ctx.CommandList);

    RenderObjectGPU* renderObjects = ctx.ResourceUploader->MapBuffer<RenderObjectGPU>({
        .Buffer = RenderObjects.Buffer,
        .Description = {
            .SizeBytes = renderObjectsSizeBytes,
            .Offset = RenderObjects.Offset
        }
    });
    IndirectDrawCommand* commands = ctx.ResourceUploader->MapBuffer<IndirectDrawCommand>({
        .Buffer = Commands.Buffer,
        .Description = {
            .SizeBytes = commandsSizeBytes,
            .Offset = Commands.Offset
        }
    });
    MeshletGPU* meshlets = ctx.ResourceUploader->MapBuffer<MeshletGPU>({
        .Buffer = Meshlets.Buffer,
        .Description = {
            .SizeBytes = meshletsSizeBytes,
            .Offset = Meshlets.Offset
        }
    });

    const u32 currentMeshletIndex = CommandCount;
    const u32 currentRenderObjectIndex = (u32)(RenderObjects.Offset / sizeof(RenderObjectGPU));
    u32 meshletIndex = 0;
    for (auto&& [renderObjectIndex, renderObject] : std::views::enumerate(sceneInfo.m_Geometry.RenderObjects))
    {
        const u32 renderObjectFirstIndex = renderObject.FirstIndex + sceneInfoOffsets.ElementOffsets[(u32)Index];
        const u32 renderObjectFirstVertex = renderObject.FirstVertex + sceneInfoOffsets.ElementOffsets[(u32)Position];
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
                .UvIndex = sceneInfoOffsets.ElementOffsets[(u32)Uv] + renderObjectFirstVertex
            }
        };

        for (u32 localMeshletIndex = 0; localMeshletIndex < renderObject.MeshletCount; localMeshletIndex++)
        {
            auto& meshlet = sceneInfo.m_Geometry.Meshlets[renderObject.FirstMeshlet + localMeshletIndex];
            commands[meshletIndex] = IndirectDrawCommand{
                {
                    .IndexCount = meshlet.IndexCount,
                    .InstanceCount = 1,
                    .FirstIndex = meshlet.FirstIndex + sceneInfoOffsets.ElementOffsets[(u32)Index] +
                    renderObjectFirstIndex,
                    .VertexOffset = (i32)meshlet.FirstVertex,
                    .FirstInstance = currentMeshletIndex + meshletIndex,
                    .RenderObject = (u32)renderObjectIndex + currentRenderObjectIndex
                }
            };
            meshlets[meshletIndex] = {
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
            };
            meshletIndex++;
        }
    }


    RenderObjects.Offset += renderObjectsSizeBytes;
    CommandCount += meshletCount;
    Commands.Offset += commandsSizeBytes;
    Meshlets.Offset += meshletsSizeBytes;

    return {
        .FirstRenderObject = currentRenderObjectIndex,
        .FirstMeshlet = currentMeshletIndex
    };
}
