#include "rendererpch.h"
#include "SceneAssetManager.h"

#include "SceneAsset.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Materials/MaterialMeta.h>
#include <AssetLib/Scenes/SceneAsset.h>
#include <AssetLib/Scenes/SceneMeta.h>
#include <AssetBakerLib/Importers/Scenes/SceneImporter.h>

namespace lux
{
void SceneAssetManager::OnAssetSystemInit()
{
    m_TextureAssetManager = m_AssetSystem->GetAssetManagerFor<ImageAssetManager>(assetlib::image::ASSET_TYPE);
    ASSERT(m_TextureAssetManager)
    
    m_MaterialAssetManager =
        m_AssetSystem->GetAssetManagerFor<MaterialAssetManager>(assetlib::material::ASSET_TYPE);
    ASSERT(m_MaterialAssetManager)    
    
    m_MaterialUpdatedHandler = AssetUpdatedHandler([this](const AssetUpdatedInfo& updatedMaterial) {
        OnMaterialUpdated(updatedMaterial.AssetHandle);
    });
    m_TextureUpdatedHandler = AssetUpdatedHandler([this](const AssetUpdatedInfo& updatedTexture) {
        OnTextureUpdated(updatedTexture.AssetHandle);
    });
    m_AssetSystem->SubscribeOnAssetUpdate(assetlib::material::ASSET_TYPE, m_MaterialUpdatedHandler);
    m_AssetSystem->SubscribeOnAssetUpdate(assetlib::image::ASSET_TYPE, m_TextureUpdatedHandler);
}

bool SceneAssetManager::AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    if (path.extension() != assetlib::ASSETLIB_METADATA_EXTENSION || !Bakes(assetlib::getMetadataRawExtension(path)))
        return false;

    auto metadataRead = assetlib::io::readBaseAssetMetadata(path);
    if (!metadataRead.has_value())
        return false;
    
    if (metadataRead->Type.Type != assetlib::scene::ASSET_TYPE)
        return false;

    resolver.RegisterId(metadataRead->AssetId, {
        .Path = metadataRead->Io.OriginalFile,
        .MetaPath = path,
        .AssetType = metadataRead->Type.Type
    });

    return true;
}

bool SceneAssetManager::Bakes(std::string_view extension)
{
    bool bakes = false;
    for (auto& rawExtension : bakers::SCENE_ASSET_RAW_EXTENSIONS)
        bakes = bakes || extension == rawExtension;

    return bakes;
}

void SceneAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Bakes(path.extension().string()) || Bakes(assetlib::getMetadataRawExtension(path)))
        OnRawFileModified(path);
}

void SceneAssetManager::SetTextureRingBuffer(BindlessTextureDescriptorsRingBuffer& ringBuffer)
{
    m_TexturesRingBuffer = &ringBuffer;
}

SceneHandle SceneAssetManager::AddExternalScene(SceneAsset&& scene)
{
    Lock lock(m_ResourceAccessMutex);
    
    return m_Scenes.Add(std::move(scene), {});
}

void SceneAssetManager::OnFrameBegin(FrameContext&)
{
    Lock lock(m_ResourceAccessMutex);
    
    auto& queued = m_ToUnload[(u32)UnloadState::Queued];
    auto& toUnload = m_ToUnload[(u32)UnloadState::Unload];

    for (SceneHandle toUnloadScene : toUnload)
        m_Scenes.Erase(toUnloadScene, {});
    toUnload.clear();
    
    for (SceneHandle queuedScene : queued)
        toUnload.push_back(queuedScene);
    queued.clear();
}

SceneHandle SceneAssetManager::LoadAsset(const SceneLoadParameters& parameters)
{
    const std::filesystem::path path = weakly_canonical(parameters.Path).generic_string();
    bakers::SceneImporter importer(m_Ctx);
    const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));
    
    const SceneHandle cached = m_Scenes.Find(id);
    if (cached.IsValid())
        return cached;
    
    auto sceneAsset = DoLoad(importer, path);
    if (!sceneAsset.has_value())
        return {};
    
    const SceneHandle newScene = m_Scenes.Add(std::move(*sceneAsset), id);
    RegisterMaterials(newScene);
    
    return newScene;
}

void SceneAssetManager::UnloadAsset(SceneHandle handle)
{
    const assetlib::AssetId id = m_Scenes.Find(handle);
    if (!id.HasValue())
        return;

    const auto* assetInfo = m_AssetSystem->Resolve(id);
    if (!assetInfo)
        return;
    
    LUX_LOG_INFO("Unloading scene: {}", assetInfo->Path.string());

    m_ToUnload[(u32)UnloadState::Queued].push_back(handle);
    UnregisterMaterials(handle);
    m_SceneDeletedSignal.Emit({.Scene = handle});
}

const SceneAsset* SceneAssetManager::GetAsset(SceneHandle handle) const
{
    return handle.IsValid() ? &m_Scenes[handle] : nullptr;
}

void SceneAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    m_AssetSystem->AddBakeRequest({
        .BakeFn = [this, path]()
        {
            bakers::SceneImporter importer(m_Ctx);
            const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));
            SceneHandle cached;
            {
                Lock lock(m_ResourceAccessMutex);
                cached = m_Scenes.Find(id);
                if (!cached.IsValid())
                    return;

                UnregisterMaterials(cached);
            }
            
            auto sceneAsset = DoLoad(importer, path);
            if (!sceneAsset.has_value())
                return;
            {
                Lock lock(m_ResourceAccessMutex);
                m_Scenes[cached] = std::move(*sceneAsset);
                RegisterMaterials(cached);
                m_SceneReplacedSignal.Emit({.Scene = cached});
            }
            
            m_AssetSystem->NotifyAssetUpdate(assetlib::scene::ASSET_TYPE, {.AssetHandle = cached});
        }
    });
}

void SceneAssetManager::OnMaterialUpdated(MaterialHandle material)
{
    Lock lock(m_ResourceAccessMutex);
    
    auto& scenes = m_MaterialToScenes[material];
    if (scenes.empty())
        return;
    
    const MaterialAsset* materialAsset = m_MaterialAssetManager->Get(material);
    
    for (const SceneHandle sceneHandle : scenes)
    {
        SceneAsset& scene = m_Scenes[sceneHandle];
        auto it = std::ranges::find_if(scene.Geometry.MaterialsCpu, [&material](const auto& materialCpu) {
            return materialCpu.Handle == material;
        });
        ASSERT(it != scene.Geometry.MaterialsCpu.end())
        if (it == scene.Geometry.MaterialsCpu.end())
            continue;
        
        const u32 materialIndex = (u32)std::distance(scene.Geometry.MaterialsCpu.begin(), it);
        scene.Geometry.Materials[materialIndex] = LoadMaterial(*it, *materialAsset);
        m_MaterialUpdatedSignal.Emit({.Scene = sceneHandle});
    }
}

void SceneAssetManager::OnTextureUpdated(ImageHandle texture)
{
    auto it = m_LoadedTextures.find(texture);
    if (it == m_LoadedTextures.end())
        return;
    
    const ImageAsset textureAsset = m_TextureAssetManager->Get(texture);
    if (!textureAsset.HasValue())
        return;
    
    m_TexturesRingBuffer->SetTexture(it->second, textureAsset);
}

void SceneAssetManager::RegisterMaterials(SceneHandle sceneHandle)
{
    const SceneAsset& scene = *GetAsset(sceneHandle);
    for (auto& material : scene.Geometry.MaterialsCpu)
        m_MaterialToScenes[material.Handle].push_back(sceneHandle);
}

void SceneAssetManager::UnregisterMaterials(SceneHandle sceneHandle)
{
    const SceneAsset& scene = *GetAsset(sceneHandle);
    for (auto& material : scene.Geometry.MaterialsCpu)
    {
        auto& scenes = m_MaterialToScenes[material.Handle];
        auto it = std::ranges::find(scenes, sceneHandle);
        ASSERT(it != scenes.end())
        if (it == scenes.end())
            continue;
        
        scenes.erase(it);
    }
}


namespace 
{
template <typename T>
void copyToVector(std::vector<T>& vec, const std::byte* data, u64 sizeBytes)
{
    ASSERT(sizeBytes % sizeof(T) == 0, "Data size in bytes is not a multiple of element size")
    ASSERT((u64)data % alignof(T) == 0, "Data is not aligned properly")

    vec.resize(sizeBytes / sizeof(T));
    memcpy(vec.data(), data, sizeBytes);
}

void loadBuffers(SceneGeometryInfo& geometry, const assetlib::SceneAsset& scene)
{
    using enum assetlib::SceneAssetBufferViewType;
    ASSERT(scene.Header.Buffers.size() == 1, "Multiple sub-scenes are not supported")
    auto& sceneBuffer = scene.BuffersData[0];
    const std::byte* bufferData = sceneBuffer.data();
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

void loadRenderObjects(SceneGeometryInfo& geometry, const assetlib::SceneAsset& scene)
{
    geometry.RenderObjects.reserve(scene.Header.Meshes.size());
    for (auto& meshInfo : scene.Header.Meshes)
    {
        ASSERT(meshInfo.Primitives.size() < 2, "Render objects with more that 1 primitives are not supported")
        for (auto& primitive : meshInfo.Primitives)
        {
            const u32 firstIndex = (u32)(scene.Header.Accessors[primitive.IndicesAccessor].OffsetBytes /
                sizeof(assetlib::SceneAssetIndexType));
            const u32 firstVertex = (u32)(
                scene.Header.Accessors[primitive.FindAttribute(
                    assetlib::SceneAssetPrimitive::ATTRIBUTE_POSITION_NAME)->Accessor].OffsetBytes /
                sizeof(glm::vec3));
            const u32 firstMeshlet = (u32)(
                scene.Header.Accessors[primitive.FindAttribute(
                    assetlib::SceneAssetPrimitive::ATTRIBUTE_MESHLET_NAME)->Accessor].OffsetBytes /
                sizeof(assetlib::SceneAssetMeshlet));
            const u32 meshletsCount = scene.Header.Accessors[primitive.FindAttribute(
                assetlib::SceneAssetPrimitive::ATTRIBUTE_MESHLET_NAME)->Accessor].Count;

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

LightType lightTypeAssetLightType(assetlib::SceneAssetLightType lightType)
{
    static_assert((u32)assetlib::SceneAssetLightType::Directional == (u32)LightType::Directional);
    static_assert((u32)assetlib::SceneAssetLightType::Point == (u32)LightType::Point);
    static_assert((u32)assetlib::SceneAssetLightType::Spot == (u32)LightType::Spot);

    return (LightType)lightType;
}
}

std::optional<SceneAsset> SceneAssetManager::DoLoad(bakers::SceneImporter& importer, const std::filesystem::path& path)
{
    ASSERT(m_TextureAssetManager)
    ASSERT(m_MaterialAssetManager)
    
    LUX_LOG_INFO("Loading scene: {}", path.string());
    
    const bool willBake = importer.NeedsBaking(path);
    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load scene: {} ({})", imported.error(), path.string());
        return std::nullopt;
    }
    
    auto& sceneAsset = importer.GetImportedScene().Asset;
    if (willBake) 
        m_AssetSystem->ScanAssetsDirectory(path.parent_path());

    return SceneAsset{
        .Geometry = LoadGeometryInfo(sceneAsset),
        .Lights = LoadLightsInfo(sceneAsset),
        .Hierarchy = LoadHierarchyInfo(sceneAsset)
    };
}

SceneGeometryInfo SceneAssetManager::LoadGeometryInfo(const assetlib::SceneAsset& scene)
{ 
    SceneGeometryInfo geometryInfo = {};
    loadBuffers(geometryInfo, scene);
    LoadMaterials(geometryInfo, scene);
    loadRenderObjects(geometryInfo, scene);
    
    return geometryInfo;
}

SceneLightInfo SceneAssetManager::LoadLightsInfo(const assetlib::SceneAsset& scene)
{
    SceneLightInfo sceneLightInfo = {};
    sceneLightInfo.Lights.reserve(scene.Header.Lights.size());

    for (auto& light : scene.Header.Lights)
        sceneLightInfo.Lights.push_back({
            .Type = lightTypeAssetLightType(light.Type),
            /* this value is irrelevant, because the transform will be set by SceneHierarchy */
            .PositionDirection = glm::vec3{0.0},
            .Color = light.Color,
            .Intensity = light.Intensity,
            .Radius = light.Range,
            // todo:
            .SpotLightData = {}
        });

    return sceneLightInfo;
}

SceneHierarchyInfo SceneAssetManager::LoadHierarchyInfo(const assetlib::SceneAsset& scene)
{
    SceneHierarchyInfo sceneHierarchy = {};

    const u32 sceneIndex = scene.Header.DefaultSubscene;
    auto& subscene = scene.Header.Subscenes[sceneIndex];
    /* if scene has many top-level nodes, we need to add dummy parent, to ensure that scene as a whole has transform */
    const bool needDummyParentNode = subscene.Nodes.size() > 1;
    
    auto& nodes = scene.Header.Nodes;
    sceneHierarchy.Nodes.reserve(nodes.size() + (u32)needDummyParentNode);
    if (needDummyParentNode)
        sceneHierarchy.Nodes.push_back({
            .Type = SceneHierarchyNodeType::Dummy,
            .Depth = 0,
            .Parent = SceneHierarchyHandle::INVALID
        });
    
    struct NodeInfo
    {
        u32 ParentIndex{SceneHierarchyHandle::INVALID};
        u32 NodeIndex{SceneHierarchyHandle::INVALID};
        u16 Depth{0};
    };
    std::queue<NodeInfo> nodesToProcess;
    for (const u32 node : subscene.Nodes)
        nodesToProcess.push({
            .ParentIndex = needDummyParentNode ? 0 : SceneHierarchyHandle::INVALID,
            .NodeIndex = node,
            .Depth = needDummyParentNode ? (u16)1 : (u16)0
        });

    while (!nodesToProcess.empty())
    {
        auto [parent, nodeIndex, depth] = nodesToProcess.front();
        nodesToProcess.pop();

        sceneHierarchy.MaxDepth = std::max(sceneHierarchy.MaxDepth, depth);

        auto& node = nodes[nodeIndex];
        SceneHierarchyNodeType type = SceneHierarchyNodeType::Dummy;
        if (node.Mesh != assetlib::SCENE_UNSET_INDEX)
            type = SceneHierarchyNodeType::Mesh;
        else if (node.Light != assetlib::SCENE_UNSET_INDEX)
            type = SceneHierarchyNodeType::Light;

        const u32 thisNodeNewIndex = (u32)sceneHierarchy.Nodes.size();
        u32 payloadIndex = ~0lu;
        switch (type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex = node.Mesh;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex = node.Light;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        
        sceneHierarchy.Nodes.push_back({
            .Type = type,
            .Depth = depth,
            .Parent = parent,
            .LocalTransform = node.Transform,
            .PayloadIndex = payloadIndex
        });

        for (u32 childNode : nodes[nodeIndex].Children)
            nodesToProcess.push({
                .ParentIndex = thisNodeNewIndex,
                .NodeIndex = childNode,
                .Depth = (u16)(depth + 1)
            });
    }
    
    return sceneHierarchy;
}

void SceneAssetManager::LoadMaterials(SceneGeometryInfo& geometry, const assetlib::SceneAsset& scene)
{
    geometry.Materials.reserve(scene.Header.Materials.size());
    geometry.MaterialsCpu.reserve(scene.Header.Materials.size());
    for (auto& material : scene.Header.Materials)
    {
        auto* materialAssetInfo = m_AssetSystem->Resolve(material.MaterialAsset);
        if (!materialAssetInfo)
            continue;
        
        auto materialHandle = m_MaterialAssetManager->LoadResource({.Path = materialAssetInfo->Path});
        if (!materialHandle.IsValid())
            continue;

        auto* materialAsset = m_MaterialAssetManager->Get(materialHandle);
        if (!materialAsset)
            continue;
        
        SceneGeometryInfo::MaterialInfo materialInfo = {
            .Handle = materialHandle,
            .BaseColorUvIndex = material.BaseColorSample.UvIndex,
            .EmissiveUvIndex = material.EmissiveSample.UvIndex,
            .NormalUvIndex = material.NormalSample.UvIndex,
            .MetallicRoughnessUvIndex = material.MetallicRoughnessSample.UvIndex,
            .OcclusionUvIndex = material.OcclusionSample.UvIndex
        };
        geometry.MaterialsCpu.push_back(materialInfo);
        
        geometry.Materials.push_back(LoadMaterial(materialInfo, *materialAsset));
    }
}

MaterialGPU SceneAssetManager::LoadMaterial(const SceneGeometryInfo::MaterialInfo& materialInfo, 
    const MaterialAsset& materialAsset)
{
    return {{
        .Albedo = materialAsset.BaseColor,
        .Metallic = materialAsset.Metallic,
        .Roughness = materialAsset.Roughness,
        .AlbedoTexture = LoadTexture(materialInfo.BaseColorUvIndex, materialAsset.BaseColorTexture,
            m_TexturesRingBuffer->GetDefaultTexture(Images::DefaultKind::White)),
        .NormalTexture = LoadTexture(materialInfo.NormalUvIndex, materialAsset.NormalTexture,
            m_TexturesRingBuffer->GetDefaultTexture(Images::DefaultKind::NormalMap)),
        .MetallicRoughnessTexture = LoadTexture(materialInfo.MetallicRoughnessUvIndex, materialAsset.MetallicRoughnessTexture,
            m_TexturesRingBuffer->GetDefaultTexture(Images::DefaultKind::White)),
        .AmbientOcclusionTexture = LoadTexture(materialInfo.OcclusionUvIndex, materialAsset.OcclusionTexture,
            m_TexturesRingBuffer->GetDefaultTexture(Images::DefaultKind::White)),
        .EmissiveTexture = LoadTexture(materialInfo.EmissiveUvIndex, materialAsset.EmissiveTexture,
            m_TexturesRingBuffer->GetDefaultTexture(Images::DefaultKind::Black))
    }};
}

TextureHandle SceneAssetManager::LoadTexture(u32 uvIndex, ImageHandle image, TextureHandle fallback)
{
    if (m_LoadedTextures.contains(image))
        return m_LoadedTextures[image];
    
    ImageAsset imageAsset = image.IsValid() ? m_TextureAssetManager->Get(image) : ImageAsset{};
    if (!imageAsset.HasValue())
        return fallback;
    
    if (uvIndex > 0)
    {
        LUX_LOG_WARN("Skipping texture {}, as it uses uv set other than 0", uvIndex);
        return fallback;
    }

    return m_LoadedTextures[image] = m_TexturesRingBuffer->AddTexture(imageAsset);
}
}
