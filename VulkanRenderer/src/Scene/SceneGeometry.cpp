#include "rendererpch.h"

#include "SceneGeometry.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include <nlohmann/json.hpp>

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Rendering/Image/ImageUtility.h"

// todo: this is code duplication
namespace glm
{
    void to_json(nlohmann::json& j, const glm::vec4& vec);
    void from_json(const nlohmann::json& j, glm::vec4& vec);
    void to_json(nlohmann::json& j, const glm::vec3& vec);
    void from_json(const nlohmann::json& j, glm::vec3& vec);
}

namespace
{
    template <typename T>
    void copyToVector(std::vector<T>& vec, u8* data, u64 sizeBytes)
    {
        ASSERT(sizeBytes % sizeof(T) == 0, "Data size in bytes is not a multiple of element size")
        ASSERT((u64)data % alignof(T) == 0, "Data is not aligned properly")
        
        vec.resize(sizeBytes / sizeof(T));
        memcpy(vec.data(), data, sizeBytes);
    }
    
    void loadBuffers(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo)
    {
        using enum assetLib::SceneInfo::BufferViewType;
        ASSERT(sceneInfo.Scene.buffers.size() == 1, "Multiple sub-scenes are not supported")
        auto& sceneBuffer = sceneInfo.Scene.buffers[0];
        u8* bufferData = sceneBuffer.data.data();
        auto& views = sceneInfo.Scene.bufferViews;

        copyToVector(geometry.Indices,
            bufferData + views[(u32)Index].byteOffset, views[(u32)Index].byteLength);
        copyToVector(geometry.Positions,
            bufferData + views[(u32)Position].byteOffset, views[(u32)Position].byteLength);
        copyToVector(geometry.Normals,
            bufferData + views[(u32)Normal].byteOffset, views[(u32)Normal].byteLength);
        copyToVector(geometry.Tangents,
            bufferData + views[(u32)Tangent].byteOffset, views[(u32)Tangent].byteLength);
        copyToVector(geometry.UVs,
            bufferData + views[(u32)Uv].byteOffset, views[(u32)Uv].byteLength);
        copyToVector(geometry.Meshlets,
            bufferData + views[(u32)Meshlet].byteOffset, views[(u32)Meshlet].byteLength);
        
        /* clear data to not store it in memory twice */
        sceneBuffer.data.clear();
    }

    Image loadTexture(const tinygltf::Model& gltf, i32 textureIndex, Format textureFormat, DeletionQueue& deletionQueue)
    {
        const tinygltf::Texture& sceneTexture = gltf.textures[textureIndex];
        const tinygltf::Image& sceneImage = gltf.images[sceneTexture.source];

        return Device::CreateImage({
            .DataSource = sceneImage.image,
            .Description = {
                .Width = (u32)sceneImage.width,
                .Height = (u32)sceneImage.height,
                .Mipmaps = Images::mipmapCount({(u32)sceneImage.width, (u32)sceneImage.height}),
                .Format = textureFormat,
                .Usage = ImageUsage::Sampled}},
            deletionQueue);
    }

    MaterialFlags materialToMaterialFlags(const tinygltf::Material& material)
    {
        MaterialFlags materialFlags = MaterialFlags::None;
        if (material.alphaMode == "OPAQUE")
            materialFlags |= MaterialFlags::Opaque;
        else if (material.alphaMode == "MASK")
            materialFlags |= MaterialFlags::AlphaMask;
        else if (material.alphaMode == "BLEND")
            materialFlags |= MaterialFlags::Translucent;

        if (material.doubleSided)
            materialFlags |= MaterialFlags::TwoSided;

        return materialFlags;
    }

    void loadMaterials(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
    {
        // todo: samplers ?

        static constexpr u32 INVALID_TEXTURE = ~0lu;
        std::vector loadedTextures(sceneInfo.Scene.textures.size(), INVALID_TEXTURE);
        auto processTexture = [&](const auto& texture, Format format, u32 fallback) -> u32 {
            if (texture.index < 0)
                return fallback;
            if (texture.texCoord > 0)
            {
                LOG("Warning skipping texture {}, as it uses uv set other that 0", texture.index);  
                return fallback;
            }
            if (loadedTextures[texture.index] == INVALID_TEXTURE)
                loadedTextures[texture.index] = texturesRingBuffer.AddTexture(
                    loadTexture(sceneInfo.Scene, texture.index, format, deletionQueue));

            return loadedTextures[texture.index];
        };
        geometry.Materials.reserve(sceneInfo.Scene.materials.size());
        geometry.MaterialsCpu.reserve(sceneInfo.Scene.materials.size());
        for (auto& material : sceneInfo.Scene.materials)
        {
            geometry.Materials.push_back({{
                .Albedo = glm::vec4{*(glm::dvec4*)material.pbrMetallicRoughness.baseColorFactor.data()},
                .Metallic = (f32)material.pbrMetallicRoughness.metallicFactor,
                .Roughness = (f32)material.pbrMetallicRoughness.roughnessFactor,
                .AlbedoTexture = processTexture(
                    material.pbrMetallicRoughness.baseColorTexture, Format::RGBA8_SRGB,
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White)),
                .NormalTexture = processTexture(
                    material.normalTexture, Format::RGBA8_UNORM,
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::NormalMap)),
                .MetallicRoughnessTexture = processTexture(
                    material.pbrMetallicRoughness.metallicRoughnessTexture, Format::RGBA8_UNORM,
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White)),
                // todo:
                .AmbientOcclusionTexture = texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::White),
                .EmissiveTexture = processTexture(
                    material.emissiveTexture, Format::RGBA8_SRGB,
                    texturesRingBuffer.GetDefaultTexture(Images::DefaultKind::Black))
            }});

            geometry.MaterialsCpu.push_back({
                .Flags = materialToMaterialFlags(material)});
        }
    }

    void loadRenderObjects(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo)
    {
        geometry.RenderObjects.reserve(sceneInfo.Scene.meshes.size());
        for (auto& meshInfo : sceneInfo.Scene.meshes)
        {
            ASSERT(meshInfo.primitives.size() < 2, "Render objects with more that 1 primitives are not supported")
            for (auto& primitive : meshInfo.primitives)
            {
                const u32 firstIndex = (u32)(sceneInfo.Scene.accessors[primitive.indices].byteOffset /
                    sizeof(assetLib::SceneInfo::IndexType));
                const u32 firstVertex = (u32)(sceneInfo.Scene.accessors[primitive.attributes["POSITION"]].byteOffset /
                    sizeof(glm::vec3));

                nlohmann::json meshletAccessorJson = nlohmann::json::parse(primitive.extras_json_string);
                const u32 firstMeshlet = (u32)((u32)meshletAccessorJson["meshlets"]["byteOffset"] /
                    sizeof(assetLib::SceneInfo::Meshlet));
                const u32 meshletsCount = meshletAccessorJson["meshlets"]["count"];
                AABB meshBoundingBox = {
                    .Min = meshletAccessorJson["bounding_box"]["min"],
                    .Max = meshletAccessorJson["bounding_box"]["max"]};
                Sphere meshBoundingSphere = {
                    .Center = meshletAccessorJson["bounding_sphere"]["center"],
                    .Radius = meshletAccessorJson["bounding_sphere"]["radius"]};

                geometry.RenderObjects.push_back({
                    .Material = (u32)std::max(primitive.material, 0),
                    .FirstIndex = firstIndex,
                    .FirstVertex = firstVertex,
                    .FirstMeshlet = firstMeshlet,
                    .MeshletCount = meshletsCount,
                    .BoundingBox = meshBoundingBox,
                    .BoundingSphere = meshBoundingSphere});
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
        assetLib::SceneInfo::BufferViewType bufferType, SceneGeometry::SceneInfoOffsets& offsets, FrameContext& ctx)
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

SceneGeometryInfo SceneGeometryInfo::FromAsset(assetLib::SceneInfo& sceneInfo,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
{
    SceneGeometryInfo geometryInfo = {};
    loadBuffers(geometryInfo, sceneInfo);
    loadMaterials(geometryInfo, sceneInfo, texturesRingBuffer, deletionQueue);
    loadRenderObjects(geometryInfo, sceneInfo);

    return geometryInfo;
}

SceneGeometry SceneGeometry::CreateEmpty(DeletionQueue& deletionQueue)
{
    SceneGeometry geometry = {};
    geometry.Attributes = Device::CreateBufferArena({
       .Buffer = Device::CreateBuffer({
           .SizeBytes = DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES,
           .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
           deletionQueue),
        .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES},
       deletionQueue);
    geometry.Indices = Device::CreateBufferArena({
        .Buffer = Device::CreateBuffer({
            .SizeBytes = DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage | BufferUsage::Source},
            deletionQueue),
        .VirtualSizeBytes = DEFAULT_ARENA_VIRTUAL_SIZE_BYTES},
        deletionQueue);
    geometry.RenderObjects.Buffer = Device::CreateBuffer({
        .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    geometry.Meshlets.Buffer = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MESHLET_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    geometry.Commands.Buffer = Device::CreateBuffer({
        .SizeBytes = DEFAULT_COMMANDS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    geometry.Materials.Buffer = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MATERIALS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);

    return geometry;
}

void SceneGeometry::Add(SceneInstance instance, FrameContext& ctx)
{
    using enum assetLib::SceneInfo::BufferViewType;

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
    using enum assetLib::SceneInfo::BufferViewType;

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
            .Offset = RenderObjects.Offset}});
    IndirectDrawCommand* commands = ctx.ResourceUploader->MapBuffer<IndirectDrawCommand>({
        .Buffer = Commands.Buffer,
        .Description = {
            .SizeBytes = commandsSizeBytes,
            .Offset = Commands.Offset}});
    MeshletGPU* meshlets = ctx.ResourceUploader->MapBuffer<MeshletGPU>({
       .Buffer = Meshlets.Buffer,
       .Description = {
           .SizeBytes = meshletsSizeBytes,
           .Offset = Meshlets.Offset}});

    const u32 currentMeshletIndex = CommandCount;
    const u32 currentRenderObjectIndex = (u32)(RenderObjects.Offset / sizeof(RenderObjectGPU));
    u32 meshletIndex = 0;
    for (auto&& [renderObjectIndex, renderObject] : std::views::enumerate(sceneInfo.m_Geometry.RenderObjects))
    {
        const u32 renderObjectFirstIndex = renderObject.FirstIndex + sceneInfoOffsets.ElementOffsets[(u32)Index];
        const u32 renderObjectFirstVertex = renderObject.FirstVertex + sceneInfoOffsets.ElementOffsets[(u32)Position];
        const u32 renderObjectMaterial = renderObject.Material + sceneInfoOffsets.MaterialOffset;
        
        renderObjects[renderObjectIndex] = {{
            /* this value is irrelevant, because the transform will be set by SceneHierarchy */
            .Transform = glm::mat4(1.0f),
            .BoundingSphere = glm::vec4(renderObject.BoundingSphere.Center, renderObject.BoundingSphere.Radius),
            .MaterialId = renderObjectMaterial,
            .PositionIndex = sceneInfoOffsets.ElementOffsets[(u32)Position] + renderObjectFirstVertex,
            .NormalIndex = sceneInfoOffsets.ElementOffsets[(u32)Normal] + renderObjectFirstVertex,
            .TangentIndex = sceneInfoOffsets.ElementOffsets[(u32)Tangent] + renderObjectFirstVertex,
            .UvIndex = sceneInfoOffsets.ElementOffsets[(u32)Uv] + renderObjectFirstVertex
        }};

        for (u32 localMeshletIndex = 0; localMeshletIndex < renderObject.MeshletCount; localMeshletIndex++)
        {
            auto& meshlet = sceneInfo.m_Geometry.Meshlets[renderObject.FirstMeshlet + localMeshletIndex];
            commands[meshletIndex] = IndirectDrawCommand{{
                .IndexCount = meshlet.IndexCount,
                .InstanceCount = 1,
                .FirstIndex = meshlet.FirstIndex + sceneInfoOffsets.ElementOffsets[(u32)Index] + renderObjectFirstIndex,
                .VertexOffset = (i32)meshlet.FirstVertex,
                .FirstInstance = currentMeshletIndex + meshletIndex,
                .RenderObject = (u32)renderObjectIndex + currentRenderObjectIndex
            }};
            meshlets[meshletIndex] = {{
                    .ConeX = meshlet.BoundingCone.AxisX,
                    .ConeY = meshlet.BoundingCone.AxisY,
                    .ConeZ = meshlet.BoundingCone.AxisZ,
                    .ConeCutoff = meshlet.BoundingCone.Cutoff,
                    .X = meshlet.BoundingSphere.Center.x,
                    .Y = meshlet.BoundingSphere.Center.y,
                    .Z = meshlet.BoundingSphere.Center.z,
                    .R = meshlet.BoundingSphere.Radius,
            }};
            meshletIndex++;
        }
    }

   
    RenderObjects.Offset += renderObjectsSizeBytes;
    CommandCount += meshletCount;
    Commands.Offset += commandsSizeBytes;
    Meshlets.Offset += meshletsSizeBytes;

    return {
        .FirstRenderObject = currentRenderObjectIndex,
        .FirstMeshlet = currentMeshletIndex};
}
