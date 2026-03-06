#include "rendererpch.h"
#include "SceneInfo.h"

#include "AssetManager.h"
#include "BindlessTextureDescriptorsRingBuffer.h"
#include "Light/Light.h"

#include "Assets/AssetSystem.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAsset.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>

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

LightType lightTypeAssetLightType(lux::assetlib::SceneAssetLightType lightType)
{
    static_assert((u32)lux::assetlib::SceneAssetLightType::Directional == (u32)LightType::Directional);
    static_assert((u32)lux::assetlib::SceneAssetLightType::Point == (u32)LightType::Point);
    static_assert((u32)lux::assetlib::SceneAssetLightType::Spot == (u32)LightType::Spot);

    return (LightType)lightType;
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

SceneHierarchyInfo SceneHierarchyInfo::FromAsset(const lux::assetlib::SceneAsset& scene)
{
    SceneHierarchyInfo sceneHierarchy = {};

    const u32 sceneIndex = scene.Header.DefaultSubscene;
    auto& subscene = scene.Header.Subscenes[sceneIndex];
    
    auto& nodes = scene.Header.Nodes;
    sceneHierarchy.Nodes.reserve(nodes.size());

    struct NodeInfo
    {
        u32 ParentIndex{SceneHierarchyHandle::INVALID};
        u32 NodeIndex{SceneHierarchyHandle::INVALID};
        u16 Depth{0};
    };
    std::queue<NodeInfo> nodesToProcess;
    for (auto& node : subscene.Nodes)
        nodesToProcess.push({.NodeIndex = (u32)node});

    while (!nodesToProcess.empty())
    {
        auto [parent, nodeIndex, depth] = nodesToProcess.front();
        nodesToProcess.pop();

        sceneHierarchy.MaxDepth = std::max(sceneHierarchy.MaxDepth, depth);

        auto& node = nodes[nodeIndex];
        SceneHierarchyNodeType type = SceneHierarchyNodeType::Dummy;
        if (node.Mesh != lux::assetlib::SCENE_UNSET_INDEX)
            type = SceneHierarchyNodeType::Mesh;
        else if (node.Light != lux::assetlib::SCENE_UNSET_INDEX)
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

SceneLightInfo SceneLightInfo::FromAsset(lux::assetlib::SceneAsset& scene)
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

void SceneLightInfo::AddLight(const DirectionalLight& light)
{
    Lights.push_back({
        .Type = LightType::Directional,
        .PositionDirection = light.Direction,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

void SceneLightInfo::AddLight(const PointLight& light)
{
    Lights.push_back({
        .Type = LightType::Point,
        .PositionDirection = light.Position,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

SceneInfo* SceneInfo::LoadFromAsset(std::string_view assetPath,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue,
    lux::AssetSystem& assetSystem,
    lux::ImageAssetManager& imageAssetManager,
    lux::MaterialAssetManager& materialAssetManager)
{
    if (SceneInfo* cached = AssetManager::GetSceneInfo(assetPath))
        return cached;
    
    SceneInfo scene = {};

    const auto assetFile = assetSystem.GetIo().ReadHeader(assetPath);
    if (!assetFile.has_value())
        return nullptr;

    auto sceneAsset = lux::assetlib::scene::readScene(*assetFile, assetSystem.GetIo(), assetSystem.GetCompressor());
    if (!sceneAsset.has_value())
        return nullptr;

    scene.m_Geometry = SceneGeometryInfo::FromAsset(*sceneAsset, texturesRingBuffer, deletionQueue, assetSystem,
        imageAssetManager, materialAssetManager);
    scene.m_Lights = SceneLightInfo::FromAsset(*sceneAsset);
    scene.m_Hierarchy = SceneHierarchyInfo::FromAsset(*sceneAsset);

    return AssetManager::AddSceneInfo(assetPath, std::move(scene));
}

void SceneInfo::AddLight(const DirectionalLight& light)
{
    const u32 lightIndex = (u32)m_Lights.Lights.size();
    m_Lights.AddLight(light);

    m_Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = m_Lights.Lights.back().GetTransform(),
        .PayloadIndex = lightIndex
    });
}

void SceneInfo::AddLight(const PointLight& light)
{
    const u32 lightIndex = (u32)m_Lights.Lights.size();
    m_Lights.AddLight(light);

    m_Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = m_Lights.Lights.back().GetTransform(),
        .PayloadIndex = lightIndex
    });
}