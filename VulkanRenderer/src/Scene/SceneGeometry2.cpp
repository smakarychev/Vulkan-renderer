#include "SceneGeometry2.h"

#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Vulkan/Device.h"

#include <nlohmann/json.hpp>

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"

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
    
    void loadBuffers(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo, DeletionQueue& deletionQueue)
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

    Image loadTexture(const tinygltf::Model& gltf, i32 textureIndex, Format textureFormat,
        std::vector<bool>& loadedTextures, DeletionQueue& deletionQueue)
    {
        const tinygltf::Texture& sceneTexture = gltf.textures[textureIndex];
        const tinygltf::Image& sceneImage = gltf.images[sceneTexture.source];

        loadedTextures[textureIndex] = true;
        
        return Device::CreateImage({
            .DataSource = sceneImage.image,
            .Description = {
                .Width = (u32)sceneImage.width,
                .Height = (u32)sceneImage.height,
                .Mipmaps = ImageUtils::mipmapCount({(u32)sceneImage.width, (u32)sceneImage.height}),
                .Format = textureFormat,
                .Usage = ImageUsage::Sampled}},
            deletionQueue);
    }

    void loadMaterials(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
    {
        // todo: samplers ?
        
        std::vector<bool> loadedTextures(sceneInfo.Scene.textures.size());
        auto processTexture = [&](const auto& texture, Format format, RenderHandle<Texture> fallback) ->
            RenderHandle<Texture> {
            if (texture.index < 0 || loadedTextures[texture.index])
                return fallback;
            if (texture.texCoord > 0)
            {
                LOG("Warning skipping texture {}, as it uses uv set other that 0", texture.index);  
                return fallback;
            }

            loadedTextures[texture.index] = true;

            return texturesRingBuffer.AddTexture(
                loadTexture(sceneInfo.Scene, texture.index, format, loadedTextures, deletionQueue));
        };
        geometry.Materials.reserve(sceneInfo.Scene.materials.size());
        for (auto& material : sceneInfo.Scene.materials)
        {
            geometry.Materials.push_back({
                .Albedo = glm::vec4{*(glm::dvec4*)material.pbrMetallicRoughness.baseColorFactor.data()},
                .Metallic = (f32)material.pbrMetallicRoughness.metallicFactor,
                .Roughness = (f32)material.pbrMetallicRoughness.roughnessFactor,
                .AlbedoTextureHandle = processTexture(
                    material.pbrMetallicRoughness.baseColorTexture, Format::RGBA8_SRGB,
                    texturesRingBuffer.GetDefaultTexture(ImageUtils::DefaultTexture::White)),
                .NormalTextureHandle = processTexture(
                    material.normalTexture, Format::RGBA8_UNORM,
                    texturesRingBuffer.GetDefaultTexture(ImageUtils::DefaultTexture::NormalMap)),
                .MetallicRoughnessTextureHandle = processTexture(
                    material.pbrMetallicRoughness.metallicRoughnessTexture, Format::RGBA8_UNORM,
                    texturesRingBuffer.GetDefaultTexture(ImageUtils::DefaultTexture::White)),
                // todo:
                .AmbientOcclusionTextureHandle = texturesRingBuffer.GetDefaultTexture(ImageUtils::DefaultTexture::White),
                .EmissiveTextureHandle = processTexture(
                    material.emissiveTexture, Format::RGBA8_SRGB,
                    texturesRingBuffer.GetDefaultTexture(ImageUtils::DefaultTexture::Black))});
        }
    }

    void loadMeshes(SceneGeometryInfo& geometry, assetLib::SceneInfo& sceneInfo)
    {
        geometry.Meshes.reserve(sceneInfo.Scene.meshes.size());
        for (auto& meshInfo : sceneInfo.Scene.meshes)
        {
            ASSERT(meshInfo.primitives.size() < 2, "Meshes with more that 1 primitives are not supported")
            for (auto& primitive : meshInfo.primitives)
            {
                const u32 firstIndex = (u32)(sceneInfo.Scene.accessors[primitive.indices].byteOffset /
                    sizeof(assetLib::ModelInfo::IndexType));
                const u32 firstVertex = (u32)(sceneInfo.Scene.accessors[primitive.attributes["POSITION"]].byteOffset /
                    sizeof(glm::vec3));

                nlohmann::json meshletAccessorJson = nlohmann::json::parse(primitive.extras_json_string);
                const u32 firstMeshlet = (u32)((u32)meshletAccessorJson["meshlets"]["byteOffset"] /
                    sizeof(assetLib::ModelInfo::Meshlet));
                const u32 meshletsCount = meshletAccessorJson["meshlets"]["count"];
                AABB meshBoundingBox = {
                    .Min = meshletAccessorJson["bounding_box"]["min"],
                    .Max = meshletAccessorJson["bounding_box"]["max"]};
                Sphere meshBoundingSphere = {
                    .Center = meshletAccessorJson["bounding_sphere"]["center"],
                    .Radius = meshletAccessorJson["bounding_sphere"]["radius"]};

                geometry.Meshes.push_back({
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

        const u64 newSize = std::max(newMinSize, (u64)GROWTH_RATE * Device::GetBufferArenaSizeBytes(arena));
        Device::ResizeBufferArena(arena, newSize, cmdList);
    }

    BufferSuballocation suballocateResizeIfFailed(BufferArena arena, u64 sizeBytes, u32 alignment,
        RenderCommandList& cmdList)
    {
        std::optional<BufferSuballocation> attributesSuballocation =
            Device::BufferArenaSuballocate(arena, sizeBytes, alignment);
        if (!attributesSuballocation)
        {
            growBufferArena(arena, Device::GetBufferArenaSizeBytes(arena) + sizeBytes, cmdList);
            attributesSuballocation = Device::BufferArenaSuballocate(arena, sizeBytes, alignment);
        }

        ASSERT(attributesSuballocation.has_value(), "Failed to suballocate")
        return *attributesSuballocation;
    }

    template <typename T>
    void writeSuballocation(BufferArena arena, const std::vector<T>& data, 
        assetLib::SceneInfo::BufferViewType bufferType, SceneGeometry2::UgbOffsets& ugbOffsets, FrameContext& ctx)
    {
        static constexpr u32 ALIGNMENT = 1;
        const BufferSuballocation suballocation = suballocateResizeIfFailed(arena,
            data.size() * sizeof(T), ALIGNMENT, ctx.CommandList);
        ugbOffsets.ElementOffsets[(u32)bufferType] = (u32)(suballocation.Description.Offset / sizeof(T));
        ctx.ResourceUploader->UpdateBuffer(suballocation.Buffer, data, suballocation.Description.Offset);
    }

    void growBuffer(Buffer buffer, u64 newMinSize, RenderCommandList& cmdList)
    {
        static constexpr f32 GROWTH_RATE = 1.5;

        const u64 newSize = std::max(newMinSize, (u64)GROWTH_RATE * Device::GetBufferSizeBytes(buffer));
        Device::ResizeBuffer(buffer, newSize, cmdList);
    }
    
    void growBufferIfNeeded(Buffer buffer, u64 requiredSize, RenderCommandList& cmdList)
    {
        const u64 currentSize = Device::GetBufferSizeBytes(buffer);
        if (requiredSize > currentSize)
            growBuffer(buffer, requiredSize, cmdList);
    }
}

SceneGeometryInfo SceneGeometryInfo::FromAsset(assetLib::SceneInfo& sceneInfo,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
{
    SceneGeometryInfo geometryInfo = {};
    loadBuffers(geometryInfo, sceneInfo, deletionQueue);
    loadMaterials(geometryInfo, sceneInfo, texturesRingBuffer, deletionQueue);
    loadMeshes(geometryInfo, sceneInfo);

    return geometryInfo;
}

SceneGeometry2 SceneGeometry2::CreateEmpty(DeletionQueue& deletionQueue)
{
    SceneGeometry2 geometry = {};
    geometry.Attributes = Device::CreateBufferArena({
       .Buffer = Device::CreateBuffer({
           .SizeBytes = DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES,
           .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
           deletionQueue)},
       deletionQueue);
    geometry.Indices = Device::CreateBufferArena({
        .Buffer = Device::CreateBuffer({
            .SizeBytes = DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage},
            deletionQueue)},
        deletionQueue);
    geometry.RenderObjects = Device::CreateBuffer({
        .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);
    geometry.Meshlets = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MESHLET_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);
    geometry.Commands = Device::CreateBuffer({
        .SizeBytes = DEFAULT_COMMANDS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage},
        deletionQueue);
    geometry.Materials = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MATERIALS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);

    return geometry;
}

void SceneGeometry2::Add(SceneInstance instance, FrameContext& ctx)
{
    using enum assetLib::SceneInfo::BufferViewType;

    auto& sceneInfo = *instance.m_SceneInfo;
    
    UgbOffsets ugbOffsets = {};
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Positions, Position, ugbOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Normals, Normal, ugbOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.Tangents, Tangent, ugbOffsets, ctx);
    writeSuballocation(Attributes, sceneInfo.m_Geometry.UVs, Uv, ugbOffsets, ctx);
    writeSuballocation(Indices, sceneInfo.m_Geometry.Indices, Index, ugbOffsets, ctx);

    const u32 meshletCount = (u32)sceneInfo.m_Geometry.Meshlets.size();
    const u64 meshletsSizeBytes = meshletCount * sizeof(assetLib::ModelInfo::Meshlet);
    growBufferIfNeeded(Meshlets,
        meshletsSizeBytes + m_MeshletsOffsetBytes,
        ctx.CommandList);
    MeshletGPU* meshlets = ctx.ResourceUploader->MapBuffer<MeshletGPU>({
        .Buffer = Meshlets,
        .Description = {
            .SizeBytes = meshletsSizeBytes,
            .Offset = m_MeshletsOffsetBytes}});
    for (auto&& [meshletIndex, meshlet] : std::ranges::views::enumerate(sceneInfo.m_Geometry.Meshlets))
        meshlets[meshletIndex] = {
            .BoundingCone = meshlet.BoundingCone,
            .BoundingSphere = {.Center = meshlet.BoundingSphere.Center, .Radius = meshlet.BoundingSphere.Radius}};

    const u64 materialsSizeBytes = sceneInfo.m_Geometry.Materials.size() * sizeof(MaterialGPU);
    growBufferIfNeeded(Materials,
        materialsSizeBytes + m_MaterialsOffsetBytes,
        ctx.CommandList);
    ctx.ResourceUploader->UpdateBuffer(Materials, sceneInfo.m_Geometry.Materials, m_MaterialsOffsetBytes);

    ugbOffsets.ElementOffsets[(u32)Meshlet] = (u32)(m_MeshletsOffsetBytes /
        sizeof(assetLib::ModelInfo::Meshlet));
    m_MeshletsOffsetBytes += meshletsSizeBytes;
    m_MaterialsOffsetBytes += materialsSizeBytes;

    m_UgbOffsets[&sceneInfo] = ugbOffsets;
}

void SceneGeometry2::AddCommands(SceneInstance instance, FrameContext& ctx)
{
    using enum assetLib::SceneInfo::BufferViewType;

    auto& sceneInfo = *instance.m_SceneInfo;
    const UgbOffsets& ugbOffsets = m_UgbOffsets[&sceneInfo];

    const u64 meshesSizeBytes = sceneInfo.m_Geometry.Meshes.size() * sizeof(RenderObjectGPU2);
    const u32 meshletCount = (u32)sceneInfo.m_Geometry.Meshlets.size();
    const u64 meshletsSizeBytes = meshletCount * sizeof(IndirectDrawCommand);
    growBufferIfNeeded(
        RenderObjects,
        meshesSizeBytes + m_RenderObjectsOffsetBytes,
        ctx.CommandList);
    CommandCount += meshletCount;
    growBufferIfNeeded(Commands,
        meshletsSizeBytes + m_CommandsOffsetBytes,
        ctx.CommandList);

    RenderObjectGPU2* renderObjects = ctx.ResourceUploader->MapBuffer<RenderObjectGPU2>({
        .Buffer = RenderObjects,
        .Description = {
            .SizeBytes = meshesSizeBytes,
            .Offset = m_RenderObjectsOffsetBytes}});
    IndirectDrawCommand* commands = ctx.ResourceUploader->MapBuffer<IndirectDrawCommand>({
        .Buffer = Commands,
        .Description = {
            .SizeBytes = meshletsSizeBytes,
            .Offset = m_CommandsOffsetBytes}});

    const u32 currentMeshletIndex = ugbOffsets.ElementOffsets[(u32)Meshlet];
    const u32 currentRenderObjectIndex = (u32)(m_RenderObjectsOffsetBytes / sizeof(RenderObjectGPU2));
    u32 meshletIndex = 0;
    for (auto&& [meshIndex, mesh] : std::ranges::views::enumerate(sceneInfo.m_Geometry.Meshes))
    {
        const u32 meshFirstIndex = mesh.FirstIndex + ugbOffsets.ElementOffsets[(u32)Index];
        const u32 meshFirstVertex = mesh.FirstVertex + ugbOffsets.ElementOffsets[(u32)Position];
        
        renderObjects[meshIndex] = {
            /* this value is irrelevant, because the transform will be set by SceneHierarchy */
            .Transform = glm::mat4(1.0f),
            .BoundingSphere = mesh.BoundingSphere,
            .MaterialGPU = mesh.Material,
            .PositionsOffset = ugbOffsets.ElementOffsets[(u32)Position] + meshFirstVertex,
            .NormalsOffset = ugbOffsets.ElementOffsets[(u32)Normal] + meshFirstVertex,
            .TangentsOffset = ugbOffsets.ElementOffsets[(u32)Tangent] + meshFirstVertex,
            .UVsOffset = ugbOffsets.ElementOffsets[(u32)Uv] + meshFirstVertex};

        for (u32 localMeshletIndex = 0; localMeshletIndex < mesh.MeshletCount; localMeshletIndex++)
        {
            auto& meshlet = sceneInfo.m_Geometry.Meshlets[mesh.FirstMeshlet + localMeshletIndex];
            commands[meshletIndex] = IndirectDrawCommand{
                .IndexCount = meshlet.IndexCount,
                .InstanceCount = 1,
                .FirstIndex = meshlet.FirstIndex + ugbOffsets.ElementOffsets[(u32)Index] + meshFirstIndex,
                .VertexOffset = (i32)meshlet.FirstVertex,
                .FirstInstance = currentMeshletIndex + meshletIndex,
                .RenderObject = (u32)meshIndex + currentRenderObjectIndex};

            meshletIndex++;
        }
    }
    
    m_RenderObjectsOffsetBytes += meshesSizeBytes;
    m_CommandsOffsetBytes += meshletsSizeBytes;
}
