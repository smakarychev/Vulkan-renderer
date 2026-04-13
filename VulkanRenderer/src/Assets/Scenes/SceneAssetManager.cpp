#include "rendererpch.h"
#include "SceneAssetManager.h"

#include "SceneAsset.h"
#include "Assets/AssetSystem.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"

#include <AssetLib/Images/ImageAsset.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Scenes/SceneAsset.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetBakerLib/Bakers/Scenes/SceneBaker.h>

namespace lux
{
bool SceneAssetManager::AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    if (path.extension() != bakers::SCENE_ASSET_EXTENSION)
        return false;

    auto assetFile = m_AssetSystem->GetIo().ReadHeader(path);
    if (!assetFile.has_value())
        return false;

    auto header = assetlib::scene::readHeader(*assetFile);
    if (!header.has_value())
        return false;

    resolver.RegisterId(assetFile->Metadata.AssetId, {
        .Path = path,
        .AssetType = assetFile->Metadata.Type
    });

    return true;
}

bool SceneAssetManager::Bakes(const std::filesystem::path& path)
{
    bool bakes = false;
    for (auto& extension : bakers::SCENE_ASSET_RAW_EXTENSIONS)
        bakes = bakes || path.extension() == extension;

    return bakes;
}

void SceneAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Bakes(path))
        OnRawFileModified(path);
}

void SceneAssetManager::Init(const bakers::SceneBakeSettings& bakeSettings)
{
    m_Context = {
        .InitialDirectory = m_AssetSystem->GetAssetsDirectory(),
        .Io = &m_AssetSystem->GetIo(),
        .Compressor = &m_AssetSystem->GetCompressor()
    };

    m_BakeSettings = &bakeSettings;
}

void SceneAssetManager::SetTextureRingBuffer(BindlessTextureDescriptorsRingBuffer& ringBuffer)
{
    m_TexturesRingBuffer = &ringBuffer;
}

void SceneAssetManager::OnFrameBegin(FrameContext&)
{
    Lock lock(m_ResourceAccessMutex);
    
    auto& queued = m_ToUnload[(u32)UnloadState::Queued];
    auto& toUnload = m_ToUnload[(u32)UnloadState::Unload];

    for (SceneHandle toUnloadScene : toUnload)
    {
        const std::filesystem::path* path = m_Scenes.Find(toUnloadScene);
        if (path == nullptr)
            continue;
        
        m_Scenes.Erase(toUnloadScene, *path);
    }
    toUnload.clear();
    
    for (SceneHandle queuedScene : queued)
        toUnload.push_back(queuedScene);
    queued.clear();
}

SceneHandle SceneAssetManager::LoadAsset(const SceneLoadParameters& parameters)
{
    const std::filesystem::path path = weakly_canonical(parameters.Path).generic_string();
    
    const SceneHandle cached = m_Scenes.Find(path);
    if (cached.IsValid())
        return cached;
    
    return m_Scenes.Add(DoLoad(parameters), path);
}

void SceneAssetManager::UnloadAsset(SceneHandle handle)
{
    const std::filesystem::path* path = m_Scenes.Find(handle);
    if (path == nullptr)
        return;

    LUX_LOG_INFO("Unloading scene: {}", path->string());

    m_ToUnload[(u32)UnloadState::Queued].push_back(handle);
    m_SceneDeletedSignal.Emit({.Scene = handle});
}

const SceneAsset* SceneAssetManager::GetAsset(SceneHandle handle) const
{
    return handle.IsValid() ? &m_Scenes[handle.Index()] : nullptr;
}

void SceneAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    m_AssetSystem->AddBakeRequest({
        .BakeFn = [this, path]()
        {
            bakers::SceneBaker baker;

            LUX_LOG_INFO("Baking scene file: {}", path.string());
            auto bakedPath = baker.BakeToFile(path, *m_BakeSettings, m_Context);
            if (!bakedPath)
            {
                LUX_LOG_WARN("Bake request failed {} ({})", bakedPath.error(), path.string());
                return;
            }
            
            OnBakedFileModified(*bakedPath);
        }
    });
}

void SceneAssetManager::OnBakedFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);

    const SceneHandle cached = m_Scenes.Find(weakly_canonical(path).generic_string());
    if (!cached.IsValid())
        return;

    const SceneHandle newScene = m_Scenes.Add(DoLoad({.Path = path}), path);
    if (!newScene.IsValid())
        return;
    
    m_ToUnload[(u32)UnloadState::Queued].push_back(cached);
    m_SceneReplacedSignal.Emit({.Original = cached, .Replaced = newScene});
}

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

void loadBuffers(SceneGeometryInfo& geometry, assetlib::SceneAsset& scene)
{
    using enum assetlib::SceneAssetBufferViewType;
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

void loadMaterials(SceneGeometryInfo& geometry, assetlib::SceneAsset& scene,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer,
    AssetSystem& assetSystem,
    ImageAssetManager& imageAssetManager,
    MaterialAssetManager& materialAssetManager)
{
    std::unordered_map<ImageAsset, TextureHandle> loadedTextures;

    auto get = [&](ImageHandle image) -> Image{
        return image.IsValid() ? imageAssetManager.Get(image) : Image{};
    };

    auto processTexture = [&](const assetlib::SceneAssetTextureSample& sample,
        ImageAsset image, TextureHandle fallback) -> TextureHandle
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

        geometry.MaterialsCpu.push_back(materialHandle);
    }
}

void loadRenderObjects(SceneGeometryInfo& geometry, assetlib::SceneAsset& scene)
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

SceneGeometryInfo loadGeometryInfo(assetlib::SceneAsset& scene,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer,
    AssetSystem& assetSystem,
    ImageAssetManager& imageAssetManager,
    MaterialAssetManager& materialAssetManager)
{
    SceneGeometryInfo geometryInfo = {};
    loadBuffers(geometryInfo, scene);
    loadMaterials(geometryInfo, scene, texturesRingBuffer, assetSystem, imageAssetManager, materialAssetManager);
    loadRenderObjects(geometryInfo, scene);
    
    return geometryInfo;
}

LightType lightTypeAssetLightType(assetlib::SceneAssetLightType lightType)
{
    static_assert((u32)assetlib::SceneAssetLightType::Directional == (u32)LightType::Directional);
    static_assert((u32)assetlib::SceneAssetLightType::Point == (u32)LightType::Point);
    static_assert((u32)assetlib::SceneAssetLightType::Spot == (u32)LightType::Spot);

    return (LightType)lightType;
}

SceneLightInfo loadLightInfo(const assetlib::SceneAsset& scene)
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

SceneHierarchyInfo loadHierarchyInfo(const assetlib::SceneAsset& scene)
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
    for (auto& node : subscene.Nodes)
        nodesToProcess.push({
            .ParentIndex = needDummyParentNode ? 0 : SceneHierarchyHandle::INVALID,
            .NodeIndex = (u32)node,
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
}

SceneAsset SceneAssetManager::DoLoad(const SceneLoadParameters& parameters) const
{
    LUX_LOG_INFO("Loading scene: {}", parameters.Path.string());
    
    const auto assetFile = m_AssetSystem->GetIo().ReadHeader(parameters.Path);
    if (!assetFile.has_value())
        return {};
    
    auto sceneAsset = assetlib::scene::readScene(*assetFile, m_AssetSystem->GetIo(), m_AssetSystem->GetCompressor());
    if (!sceneAsset.has_value())
        return {};
    
    ImageAssetManager* imageAssetManager =
        m_AssetSystem->GetAssetManagerFor<ImageAssetManager>(assetlib::image::getMetadata().Type);
    ASSERT(imageAssetManager)
    
    MaterialAssetManager* materialAssetManager =
        m_AssetSystem->GetAssetManagerFor<MaterialAssetManager>(assetlib::material::getMetadata().Type);
    ASSERT(materialAssetManager)

    return SceneAsset{
        .Geometry = loadGeometryInfo(*sceneAsset, *m_TexturesRingBuffer, 
            *m_AssetSystem, *imageAssetManager, *materialAssetManager),
        .Lights = loadLightInfo(*sceneAsset),
        .Hierarchy = loadHierarchyInfo(*sceneAsset)
    };
}
}
