#include "Scene.h"

#include <json.hpp>
#include <ranges>

#include "AssetManager.h"
#include "BindlessTextureDescriptorsRingBuffer.h"
#include "ResourceUploader.h"
#include "SceneAsset.h"
#include "Vulkan/Device.h"

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

    BufferSubresourceDescription loadBufferSubresource(const tinygltf::Model& gltf, i32 accessorIndex)
    {
        auto& accessor = gltf.accessors[accessorIndex];
        auto& bufferView = gltf.bufferViews[accessor.bufferView];
        // todo: this is duplicated
        const u64 elementSizeBytes =
            (u64)tinygltf::GetNumComponentsInType(accessor.type) *
            (u64)tinygltf::GetComponentSizeInBytes(accessor.componentType);
        const u64 sizeBytes = elementSizeBytes * accessor.count;

        return {
            .SizeBytes = sizeBytes,
            .Offset = accessor.byteOffset + bufferView.byteOffset};
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

SceneInfo* SceneInfo::LoadFromAsset(std::string_view assetPath,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
{
    if (SceneInfo* cached = AssetManager::GetSceneInfo(assetPath))
        return cached;
    
    SceneInfo scene = {};

    assetLib::SceneInfo sceneInfo = *assetLib::readSceneHeader(assetPath);
    assetLib::readSceneBinary(sceneInfo);

    LoadBuffers(scene, sceneInfo, deletionQueue);
    LoadMaterials(scene, sceneInfo, texturesRingBuffer, deletionQueue);
    LoadMeshes(scene, sceneInfo);

    return AssetManager::AddSceneInfo(assetPath, std::move(scene));
}

void SceneInfo::LoadBuffers(SceneInfo& scene, assetLib::SceneInfo& sceneInfo, DeletionQueue& deletionQueue)
{
    ASSERT(sceneInfo.Scene.buffers.size() == 1, "Multiple sub-scenes are not supported")
    auto& sceneBuffer = sceneInfo.Scene.buffers[0];
    scene.m_Buffer = Device::CreateBuffer({
        .SizeBytes = sceneBuffer.byte_length,
        .Usage = BufferUsage::Source | BufferUsage::Mappable,
        .PersistentMapping = true,
        .InitialData = sceneBuffer.data},
        deletionQueue);
    /* clear data to not store it in memory twice, as it is already copied into m_GeometryBuffer */
    sceneBuffer.data.clear();

    auto& bufferViews = sceneInfo.Scene.bufferViews;
    for (u32 i = 0; i < (u32)assetLib::SceneInfo::BufferViewType::MaxVal; i++)
        scene.m_Views[i] = {
            .SizeBytes = bufferViews[i].byteLength,
            .Offset = bufferViews[i].byteOffset};
}

void SceneInfo::LoadMaterials(SceneInfo& scene, assetLib::SceneInfo& sceneInfo,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
{
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
    scene.m_Materials.reserve(sceneInfo.Scene.materials.size());
    for (auto& material : sceneInfo.Scene.materials)
    {
        scene.m_Materials.push_back({
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

void SceneInfo::LoadMeshes(SceneInfo& scene, assetLib::SceneInfo& sceneInfo)
{
    scene.m_Meshes.reserve(sceneInfo.Scene.meshes.size());
    for (auto& meshInfo : sceneInfo.Scene.meshes)
    {
        for (auto& primitive : meshInfo.primitives)
        {
            BufferSubresource indexBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = loadBufferSubresource(sceneInfo.Scene, primitive.indices)};
            BufferSubresource positionBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = loadBufferSubresource(sceneInfo.Scene, primitive.attributes["POSITION"])};
            BufferSubresource normalsBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = loadBufferSubresource(sceneInfo.Scene, primitive.attributes["NORMAL"])};
            BufferSubresource tangentsBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = loadBufferSubresource(sceneInfo.Scene, primitive.attributes["TANGENT"])};
            BufferSubresource uvsBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = loadBufferSubresource(sceneInfo.Scene, primitive.attributes["TEXCOORD_0"])};

            nlohmann::json meshletAccessorJson = nlohmann::json::parse(primitive.extras_json_string);
            auto& meshletsView = sceneInfo.Scene.bufferViews[meshletAccessorJson["meshlets"]["bufferView"]];
            u64 meshletsSize = (u64)meshletAccessorJson["meshlets"]["count"] * sizeof(assetLib::ModelInfo::Meshlet);
            u64 meshletsOffset = meshletsView.byteOffset + (u64)meshletAccessorJson["meshlets"]["byteOffset"];
            BufferSubresource meshletsBuffer = {
                .Buffer = scene.m_Buffer,
                .Description = {
                    .SizeBytes = meshletsSize,
                    .Offset = meshletsOffset}};

            AABB meshBoundingBox = {
                .Min = meshletAccessorJson["bounding_box"]["min"],
                .Max = meshletAccessorJson["bounding_box"]["max"]};
            Sphere meshBoundingSphere = {
                .Center = meshletAccessorJson["bounding_sphere"]["center"],
                .Radius = meshletAccessorJson["bounding_sphere"]["radius"]};

            scene.m_Meshes.push_back({
                .Material = (u32)primitive.material,
                .Indices = indexBuffer,
                .Positions = positionBuffer,
                .Normals = normalsBuffer,
                .Tangents = tangentsBuffer,
                .UVs = uvsBuffer,
                .Meshlets = meshletsBuffer,
                .BoundingBox = meshBoundingBox,
                .BoundingSphere = meshBoundingSphere});
        }
    }
}

Scene Scene::CreateEmpty(DeletionQueue& deletionQueue)
{
    Scene scene = {};

    scene.m_Geometry.Attributes = Device::CreateBufferArena({
        .Buffer = Device::CreateBuffer({
            .SizeBytes = DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
            deletionQueue)},
        deletionQueue);
    scene.m_Geometry.Indices = Device::CreateBufferArena({
        .Buffer = Device::CreateBuffer({
            .SizeBytes = DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES,
            .Usage = BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage},
            deletionQueue)},
        deletionQueue);
    scene.m_Geometry.RenderObjects = Device::CreateBuffer({
        .SizeBytes = DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);
    scene.m_Geometry.Meshlets = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MESHLET_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);
    scene.m_Geometry.Commands = Device::CreateBuffer({
        .SizeBytes = DEFAULT_COMMANDS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage},
        deletionQueue);
    scene.m_Geometry.Materials = Device::CreateBuffer({
        .SizeBytes = DEFAULT_MATERIALS_BUFFER_SIZE_BYTES,
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage},
        deletionQueue);

    return scene;
}

SceneInstance Scene::Instantiate(const SceneInfo& sceneInfo, RenderCommandList& cmdList, ResourceUploader& uploader)
{
    using enum assetLib::SceneInfo::BufferViewType;

    if (m_SceneInstancesMap[&sceneInfo] == 0)
        InitGeometry(sceneInfo, cmdList, uploader);
    const SceneInfoGeometry& sceneInfoGeometry = m_SceneInfoGeometry[&sceneInfo];

    growBufferIfNeeded(
        m_Geometry.RenderObjects,
        sceneInfo.m_Meshes.size() * sizeof(RenderObjectGPU2) + m_Geometry.RenderObjectsOffsetBytes,
        cmdList);
    const u32 meshletCount = (u32)(sceneInfo.m_Views[(u32)Meshlet].SizeBytes / sizeof(assetLib::ModelInfo::Meshlet));
    m_Geometry.CommandCount += meshletCount;
    growBufferIfNeeded(m_Geometry.Commands,
        meshletCount * sizeof(IndirectDrawCommand) + m_Geometry.CommandsOffsetBytes,
        cmdList);

    RenderObjectGPU2* renderObjects = uploader.MapBuffer<RenderObjectGPU2>({
        .Buffer = m_Geometry.RenderObjects,
        .Description = {
            .SizeBytes = sceneInfo.m_Meshes.size() * sizeof(RenderObjectGPU2),
            .Offset = m_Geometry.RenderObjectsOffsetBytes}});
    IndirectDrawCommand* commands = uploader.MapBuffer<IndirectDrawCommand>({
        .Buffer = m_Geometry.Commands,
        .Description = {
            .SizeBytes = meshletCount * sizeof(IndirectDrawCommand),
            .Offset = m_Geometry.CommandsOffsetBytes}});

    const u32 currentMeshletIndex = sceneInfoGeometry.ElementOffsets[(u32)Meshlet];
    const u32 currentRenderObjectIndex = (u32)(m_Geometry.RenderObjectsOffsetBytes / sizeof(RenderObjectGPU2));
    u32 meshletIndex = 0;
    for (auto&& [meshIndex, mesh] : std::ranges::views::enumerate(sceneInfo.m_Meshes))
    {
        const u32 meshFirstIndex = (u32)((mesh.Indices.Description.Offset - sceneInfo.m_Views[(u32)Index].Offset) /
            sizeof(assetLib::ModelInfo::IndexType)) + sceneInfoGeometry.ElementOffsets[(u32)Index];
        const u32 meshFirstVertex = (u32)(
            (mesh.Positions.Description.Offset - sceneInfo.m_Views[(u32)Position].Offset) /
            sizeof(glm::vec3)) + sceneInfoGeometry.ElementOffsets[(u32)Position];
        
        renderObjects[meshIndex] = {
            // todo: actual transform
            .Transform = glm::mat4(1.0f),
            .BoundingSphere = mesh.BoundingSphere,
            .MaterialGPU = mesh.Material,
            .PositionsOffset = sceneInfoGeometry.ElementOffsets[(u32)Position] + meshFirstVertex,
            .NormalsOffset = sceneInfoGeometry.ElementOffsets[(u32)Normal] + meshFirstVertex,
            .TangentsOffset = sceneInfoGeometry.ElementOffsets[(u32)Tangent] + meshFirstVertex,
            .UVsOffset = sceneInfoGeometry.ElementOffsets[(u32)Uv] + meshFirstVertex};

        const Span meshletsView = Device::GetMappedBufferView<const assetLib::ModelInfo::Meshlet>(mesh.Meshlets);
        for (auto& meshlet : meshletsView)
        {
            commands[meshletIndex] = IndirectDrawCommand{
                .IndexCount = meshlet.IndexCount,
                .InstanceCount = 1,
                .FirstIndex = meshlet.FirstIndex + sceneInfoGeometry.ElementOffsets[(u32)Index] + meshFirstIndex,
                .VertexOffset = (i32)meshlet.FirstVertex,
                .FirstInstance = currentMeshletIndex + meshletIndex,
                .RenderObject = (u32)meshIndex + currentRenderObjectIndex};

            meshletIndex++;
        }
    }
    
    m_Geometry.RenderObjectsOffsetBytes += sceneInfo.m_Meshes.size() * sizeof(RenderObjectGPU2);
    m_Geometry.CommandsOffsetBytes += meshletCount * sizeof(IndirectDrawCommand);

    return RegisterSceneInstance(sceneInfo);
}

void Scene::InitGeometry(const SceneInfo& sceneInfo, RenderCommandList& cmdList, ResourceUploader& uploader)
{
    using enum assetLib::SceneInfo::BufferViewType;

    SceneInfoGeometry sceneGeometry = {};
    for (u32 i = 0; i < (u32)Index; i++)
    {
        u32 typeSize = sizeof(glm::vec3);
        if (i == (u32)Tangent)
            typeSize = sizeof(glm::vec4);
        if (i == (u32)Uv)
            typeSize = sizeof(glm::vec2);
        
        const BufferSuballocation suballocation = suballocateResizeIfFailed(m_Geometry.Attributes,
            sceneInfo.m_Views[i].SizeBytes, 1, cmdList);
        sceneGeometry.ElementOffsets[i] = (u32)(suballocation.Description.Offset / typeSize);
        uploader.CopyBuffer({
            .Source = sceneInfo.m_Buffer,
            .Destination = suballocation.Buffer,
            .SizeBytes = sceneInfo.m_Views[i].SizeBytes,
            .SourceOffset = sceneInfo.m_Views[i].Offset,
            .DestinationOffset = suballocation.Description.Offset});
    }

    static constexpr u32 INDEX_TYPE_SIZE = sizeof(assetLib::ModelInfo::IndexType);
    const BufferSuballocation indicesSuballocation = suballocateResizeIfFailed(m_Geometry.Indices,
        sceneInfo.m_Views[(u32)Index].SizeBytes, 1, cmdList);
    sceneGeometry.ElementOffsets[(u32)Index] = (u32)(indicesSuballocation.Description.Offset / INDEX_TYPE_SIZE);
    uploader.CopyBuffer({
        .Source = sceneInfo.m_Buffer,
        .Destination = indicesSuballocation.Buffer,
        .SizeBytes = sceneInfo.m_Views[(u32)Index].SizeBytes,
        .SourceOffset = sceneInfo.m_Views[(u32)Index].Offset,
        .DestinationOffset = indicesSuballocation.Description.Offset});
    
    const u32 meshletCount = (u32)(sceneInfo.m_Views[(u32)Meshlet].SizeBytes / sizeof(assetLib::ModelInfo::Meshlet));

    const u64 meshletsSizeBytes = meshletCount * sizeof(assetLib::ModelInfo::Meshlet);
    growBufferIfNeeded(m_Geometry.Meshlets,
        meshletsSizeBytes + m_Geometry.MeshletsOffsetBytes,
        cmdList);
    const Span meshletsView = Device::GetMappedBufferView<const assetLib::ModelInfo::Meshlet>({
        .Buffer = sceneInfo.m_Buffer,
        .Description = sceneInfo.m_Views[(u32)Meshlet]});
    MeshletGPU* meshlets = uploader.MapBuffer<MeshletGPU>({
        .Buffer = m_Geometry.Meshlets,
        .Description = {
            .SizeBytes = meshletsSizeBytes,
            .Offset = m_Geometry.MeshletsOffsetBytes}});
    for (auto&& [meshletIndex, meshlet] : std::ranges::views::enumerate(meshletsView))
        meshlets[meshletIndex] = {
            .BoundingCone = meshlet.BoundingCone,
            .BoundingSphere = {.Center = meshlet.BoundingSphere.Center, .Radius = meshlet.BoundingSphere.Radius}};

    const u64 materialsSizeBytes = sceneInfo.m_Materials.size() * sizeof(MaterialGPU);
    growBufferIfNeeded(m_Geometry.Materials,
        materialsSizeBytes + m_Geometry.MaterialsOffsetBytes,
        cmdList);
    uploader.UpdateBuffer(m_Geometry.Materials, sceneInfo.m_Materials, m_Geometry.MaterialsOffsetBytes);

    sceneGeometry.ElementOffsets[(u32)Meshlet] = (u32)(m_Geometry.MeshletsOffsetBytes /
        sizeof(assetLib::ModelInfo::Meshlet));
    m_Geometry.MeshletsOffsetBytes += meshletsSizeBytes;
    m_Geometry.MaterialsOffsetBytes += materialsSizeBytes;

    m_SceneInfoGeometry[&sceneInfo] = sceneGeometry;
}

SceneInstance Scene::RegisterSceneInstance(const SceneInfo& sceneInfo)
{
    m_SceneInstancesMap[&sceneInfo] += 1;
    SceneInstance instance = {};
    instance.m_InstanceId = m_ActiveInstances;
    instance.m_SceneInfo = &sceneInfo;
    
    m_ActiveInstances++;
    
    return instance;
}
