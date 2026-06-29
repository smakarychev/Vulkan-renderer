#include "rendererpch.h"
#include "SceneAssetManager.h"

#include "SceneAsset.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Materials/MaterialMeta.h>
#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <AssetLib/Scenes/Scene/SceneMeta.h>
#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferAsset.h>
#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferMeta.h>
#include <AssetLib/Scenes/Mesh/MeshAsset.h>
#include <AssetLib/Scenes/Mesh/MeshMeta.h>
#include <AssetImportLib/Importers/Import.h>
#include <AssetImportLib/Importers/Scenes/SceneImporter.h>
#include <AssetImportLib/Importers/Scenes/GeometryBufferImporter.h>
#include <AssetImportLib/Importers/Scenes/MeshImporter.h>

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

bool SceneAssetManager::AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path&)
{
    return
        metadata.Type.Type == assetlib::scene::ASSET_TYPE ||
        metadata.Type.Type == assetlib::sceneGeometry::ASSET_TYPE ||
        metadata.Type.Type == assetlib::sceneMesh::ASSET_TYPE;
}

bool SceneAssetManager::Imports(std::string_view extension)
{
    bool bakes = false;
    for (auto& rawExtension : import::SCENE_ASSET_RAW_EXTENSIONS)
        bakes = bakes || extension == rawExtension;

    return bakes;
}

void SceneAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Imports(path.extension().string()) || Imports(assetlib::getMetadataRawExtension(path)))
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
    import::SceneImporter importer(m_Ctx);
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
    m_AssetSystem->AddImportRequest({
        .ImportFn = [this, path]()
        {
            import::SceneImporter importer(m_Ctx);
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
LightType lightTypeAssetLightType(assetlib::SceneAssetLightType lightType)
{
    static_assert((u32)assetlib::SceneAssetLightType::Directional == (u32)LightType::Directional);
    static_assert((u32)assetlib::SceneAssetLightType::Point == (u32)LightType::Point);
    static_assert((u32)assetlib::SceneAssetLightType::Spot == (u32)LightType::Spot);

    return (LightType)lightType;
}
}

std::optional<SceneAsset> SceneAssetManager::DoLoad(import::SceneImporter& importer, const std::filesystem::path& path)
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

    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset> importedBuffers;
    std::optional<SceneGeometryInfo> geometryInfo = LoadGeometryInfo(sceneAsset, importedBuffers);
    if (!geometryInfo.has_value())
    {
        LUX_LOG_ERROR("Failed to load geometry info for scene: {}", path.string());
        return std::nullopt;
    }
    
    SceneHierarchyInfo hierarchyInfo = LoadHierarchyInfo(sceneAsset, *geometryInfo, importedBuffers);
    
    return SceneAsset{
        .Geometry = std::move(*geometryInfo),
        .Lights = LoadLightsInfo(sceneAsset),
        .Hierarchy = std::move(hierarchyInfo)
    };
}

std::optional<SceneGeometryInfo> SceneAssetManager::LoadGeometryInfo(const assetlib::SceneAsset& scene, 
    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset>& importedBuffers)
{ 
    SceneGeometryInfo geometryInfo = {};
    
    if (auto loadMeshesResult = LoadMeshesAndSkins(geometryInfo, scene, importedBuffers); !loadMeshesResult)
        return std::nullopt;
    
    return geometryInfo;
}

SceneLightInfo SceneAssetManager::LoadLightsInfo(const assetlib::SceneAsset& scene)
{
    SceneLightInfo sceneLightInfo = {};
    sceneLightInfo.Lights.reserve(scene.Lights.size());

    for (auto& light : scene.Lights)
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

SceneHierarchyInfo SceneAssetManager::LoadHierarchyInfo(const assetlib::SceneAsset& scene, 
    SceneGeometryInfo& geometryInfo, 
    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset>& importedBuffers)
{
    SceneHierarchyInfo sceneHierarchy = {};
    
    LoadAnimations(sceneHierarchy, scene, importedBuffers);
    LoadNodes(sceneHierarchy, scene, geometryInfo);
    
    return sceneHierarchy;
}

void SceneAssetManager::LoadNodes(SceneHierarchyInfo& sceneHierarchy, const assetlib::SceneAsset& scene,
    const SceneGeometryInfo& geometryInfo)
{
    const u32 sceneIndex = scene.DefaultSubscene;
    auto& subscene = scene.Subscenes[sceneIndex];
    /* if scene has many top-level nodes, we need to add dummy parent, to ensure that scene as a whole has transform */
    const bool needDummyParentNode = subscene.Nodes.size() > 1;

    auto& nodes = scene.Nodes;
    sceneHierarchy.Nodes.reserve(nodes.size() + (u32)needDummyParentNode);
    if (needDummyParentNode)
        sceneHierarchy.Nodes.push_back({
            .Type = SceneHierarchyNodeType::Dummy,
            .Depth = 0,
            .Parent = SceneHierarchyHandle::INVALID
        });
    
    const u32 jointCount = std::ranges::fold_left(scene.Skins, 0u, [](u32 sum, auto& skin) {
        return sum + (u32)skin.JointNodes.size();
    });
    sceneHierarchy.Joints.reserve(jointCount);
    
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
    
    std::vector<u32> nodesReorder(sceneHierarchy.Nodes.capacity());

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
        SceneHierarchyPayload payload = {};
        
        switch (type)
        {
        case SceneHierarchyNodeType::Mesh:
            {
                auto& meshRenderObjects = geometryInfo.MeshRenderObjects[node.Mesh];
                auto& firstRenderObject = geometryInfo.RenderObjects[meshRenderObjects.FirstRenderObject];
                payload.Mesh = {
                    .FirstRenderObject = meshRenderObjects.FirstRenderObject,
                    .RenderObjectCount = meshRenderObjects.RenderObjectCount,
                    .FirstBlendShape = firstRenderObject.BlendShapeIndex
                };
            }
            break;
        case SceneHierarchyNodeType::Light:
            payload.Light.Index = node.Light;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        
        nodesReorder[nodeIndex] = (u32)thisNodeNewIndex;
        sceneHierarchy.Nodes.push_back({
            .Type = type,
            .Depth = depth,
            .Parent = parent,
            .LocalTransform = node.Transform,
            .Payload = payload
        });

        for (u32 childNode : nodes[nodeIndex].Children)
            nodesToProcess.push({
                .ParentIndex = thisNodeNewIndex,
                .NodeIndex = childNode,
                .Depth = (u16)(depth + 1)
            });
    }
    
    u32 jointIndex = 0;
    for (auto& skinJoint : geometryInfo.SkinJoints)
    {
        for (auto& joint : skinJoint.JointNodes)
        {
            sceneHierarchy.Joints.push_back({
                .Node = SceneHierarchyHandle{.Handle = nodesReorder[joint]},
                .JointMatrixIndex = jointIndex,
                .InverseBindMatrix = geometryInfo.JointInverseBindMatrices[jointIndex]
            });
            jointIndex += 1;
        }
    }
    
    for (auto& animation : sceneHierarchy.Animations)
        animation.Node = SceneHierarchyHandle{.Handle = nodesReorder[animation.Node.Handle]};
}

void SceneAssetManager::LoadAnimations(SceneHierarchyInfo& sceneHierarchy, const assetlib::SceneAsset& scene,
    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset>& importedBuffers)
{
    auto copyAccessorToVector = [](auto& vector, const assetlib::GeometryBufferAccessor& accessor,
        const assetlib::GeometryBufferAsset& buffer, u64 accessorElementSizeBytes, u64 vectorElementSizeBytes) {
        vector.resize(accessor.Count);
        
        if (accessorElementSizeBytes == vectorElementSizeBytes)
        {
            std::memcpy(vector.data(),
                buffer.Data.data() + buffer.Header.BufferViews[accessor.BufferView].OffsetBytes +
                accessor.OffsetBytes, accessor.Count * sizeof(vector[0]));
            return;
        }
        
        const std::byte* source = buffer.Data.data() + buffer.Header.BufferViews[accessor.BufferView].OffsetBytes +
            accessor.OffsetBytes;
        std::byte* destination = (std::byte*)vector.data();
        
        for (u32 i = 0; i < accessor.Count; i++)
        {
            std::memcpy(destination, source, accessorElementSizeBytes);
            source += accessorElementSizeBytes;
            destination += vectorElementSizeBytes;
        }
    };
    
    for (auto& animation : scene.Animations)
    {
        // todo: this loads extra stuff that we do not need for animations, consider another lighter function
        auto* bufferInfo = GetGeometryBuffer(animation.GeometryBuffer, importedBuffers);
        if (!bufferInfo)
            continue;
        
        auto& accessors = bufferInfo->Header.Accessors;
        
        for (auto& channel : animation.Channels)
        {
            auto& timestampsAccessor = accessors[channel.TimestampsAccessor];                
            auto& keyframesAccessor = accessors[channel.KeyframesAccessor];    
            ASSERT(timestampsAccessor.Count == keyframesAccessor.Count / channel.KeyframeElementCount)

            auto sceneAnimationIt = std::ranges::find_if(sceneHierarchy.Animations, [&channel](auto& animation) {
                return animation.Node == channel.TargetNode;
            });
            if (sceneAnimationIt == sceneHierarchy.Animations.end())
            {
                sceneHierarchy.Animations.push_back({
                    .Name = StringId::FromString(animation.Name),
                    .Node = channel.TargetNode,
                });
                sceneAnimationIt = std::prev(sceneHierarchy.Animations.end()); 
            }
            
            const SceneHierarchyAnimationChannelType channelType = animationChannelTypeFromAssetAnimationChannelType(
                channel.Type);
            const SceneHierarchyAnimationSamplerType samplerType = animationSamplerTypeFromAssetAnimationSamplerType(
                channel.SamplerType);

            SceneHierarchyAnimationChannel animationChannel(channelType, samplerType, channel.KeyframeElementCount);
            
            copyAccessorToVector(animationChannel.TimestampsMutable(), timestampsAccessor, *bufferInfo, 
                sizeof(f32), sizeof(f32));

            switch (channelType) 
            {
            case SceneHierarchyAnimationChannelType::Translation:
                {
                    copyAccessorToVector(animationChannel.KeyframesMutable(), keyframesAccessor, *bufferInfo,
                        sizeof(glm::vec3), sizeof(animationChannel.KeyframesMutable()[0]));
                    sceneAnimationIt->TranslationChannel = 
                        sceneHierarchy.AnimationChannels.insert(std::move(animationChannel));
                    break;
                }
            case SceneHierarchyAnimationChannelType::Orientation:
                {
                    copyAccessorToVector(animationChannel.KeyframesMutable(), keyframesAccessor, *bufferInfo,
                        sizeof(glm::quat), sizeof(animationChannel.KeyframesMutable()[0]));
                    sceneAnimationIt->OrientationChannel =  
                        sceneHierarchy.AnimationChannels.insert(std::move(animationChannel));
                    break;
                }
            case SceneHierarchyAnimationChannelType::Scale:
                {
                    copyAccessorToVector(animationChannel.KeyframesMutable(), keyframesAccessor, *bufferInfo,
                        sizeof(glm::vec3), sizeof(animationChannel.KeyframesMutable()[0]));
                    sceneAnimationIt->ScaleChannel =  
                        sceneHierarchy.AnimationChannels.insert(std::move(animationChannel));
                    break;
                }
            case SceneHierarchyAnimationChannelType::Weight:
                {
                    copyAccessorToVector(animationChannel.KeyframesMutable(), keyframesAccessor, *bufferInfo,
                        sizeof(f32), sizeof(animationChannel.KeyframesMutable()[0]));
                    sceneAnimationIt->WeightChannel =  
                        sceneHierarchy.AnimationChannels.insert(std::move(animationChannel));
                    break;
                }
            default:
                ASSERT(false)
            }
        }
    }
}

bool SceneAssetManager::LoadMeshesAndSkins(SceneGeometryInfo& geometry, const assetlib::SceneAsset& scene, 
    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset>& importedBuffers)
{
    struct ImportedMeshInfo
    {
        assetlib::GeometryBufferAsset* Buffer{nullptr};
        assetlib::MeshAsset MeshAsset{};
        std::vector<u32> PrimitivesSkinIndices;
    };
    struct ImportedSkinInfo
    {
        assetlib::GeometryBufferAsset* Buffer{nullptr};
        u32 FirstJointMatrix{SceneSkin::INVALID};
        bool IsLoaded() const { return FirstJointMatrix != SceneSkin::INVALID; }
    };
    struct SkinInfo
    {
        u32 SkinIndex{};
        u32 MeshIndex{};
        u32 PrimitiveIndex{};
    };
    std::unordered_map<assetlib::AssetId, u32> importedMaterials;
    std::vector<ImportedMeshInfo> importedMeshes;
    std::vector<ImportedSkinInfo> importedSkins;
    std::vector<SkinInfo> skins;
    u32 totalRenderObjectCount{0};
    u32 totalSkinsCount{0};
    u32 totalBlendShapeCount{0};

    auto getMaterialIndex = [&importedMaterials, &geometry, this](const assetlib::MeshPrimitiveMaterial& material) ->
        u32
    {
        if (!importedMaterials.contains(material.MaterialAsset))
        {
            auto* materialAssetInfo = m_AssetSystem->Resolve(material.MaterialAsset);
            if (!materialAssetInfo)
                return ~0lu;
        
            auto materialHandle = m_MaterialAssetManager->LoadResource({.Path = materialAssetInfo->Path});
            if (!materialHandle.IsValid())
                return ~0lu;

            auto* materialAsset = m_MaterialAssetManager->Get(materialHandle);
            if (!materialAsset)
                return ~0lu;
                
            SceneGeometryInfo::MaterialInfo materialInfo = {
                .Handle = materialHandle,
                .BaseColorUvIndex = material.BaseColorSample.UvIndex,
                .EmissiveUvIndex = material.EmissiveSample.UvIndex,
                .NormalUvIndex = material.NormalSample.UvIndex,
                .MetallicRoughnessUvIndex = material.MetallicRoughnessSample.UvIndex,
                .OcclusionUvIndex = material.OcclusionSample.UvIndex
            };
            const u32 materialIndex = (u32)geometry.Materials.size();
            geometry.MaterialsCpu.push_back(materialInfo);
            geometry.Materials.push_back(LoadMaterial(materialInfo, *materialAsset));
                
            importedMaterials.emplace(material.MaterialAsset, materialIndex);
        }
        
        return importedMaterials.at(material.MaterialAsset);
    };
    auto preload = [&]() -> bool {
        importedMeshes.reserve(scene.Meshes.size());
        importedSkins.reserve(scene.Skins.size());
        skins.reserve(scene.Meshes.size());
        
        for (assetlib::AssetId mesh : scene.Meshes)
        {
            auto* meshAssetInfo = m_AssetSystem->Resolve(mesh);
            if (!meshAssetInfo)
                return false;
    
            import::MeshImporter importer(m_Ctx);
            auto imported = importer.Import(meshAssetInfo->MetaPath);
            if (!imported.has_value())
                return false;
    
            auto& importedMesh = importer.GetImportedMesh().Asset;
            auto* bufferInfo = GetGeometryBuffer(importedMesh.GeometryBuffer, importedBuffers);
            if (!bufferInfo)
                return false;
        
            totalRenderObjectCount += (u32)importedMesh.Primitives.size();
            totalBlendShapeCount += std::ranges::fold_left(importedMesh.Primitives, 0u, [](u32 sum, auto& primitive) {
                return sum + (u32)primitive.BlendShapes.size();
            });
            importedMeshes.push_back({.Buffer = bufferInfo, .MeshAsset = importedMesh, .PrimitivesSkinIndices = {}});
        }

        for (auto& skin : scene.Skins)
        {
            auto* bufferInfo = GetGeometryBuffer(skin.GeometryBuffer, importedBuffers);
            if (!bufferInfo)
                return false;
            importedSkins.push_back({.Buffer = bufferInfo});
        }
        
        for (auto& node : scene.Nodes)
        {
            if (node.Skin != assetlib::SCENE_UNSET_INDEX)
            {
                auto& mesh = importedMeshes[node.Mesh];
                const u32 primitiveCount = (u32)mesh.MeshAsset.Primitives.size();
                mesh.PrimitivesSkinIndices.resize(primitiveCount);
                for (u32 i = 0; i < primitiveCount; i++)
                {
                    const u32 skinIndex = (u32)skins.size();
                    skins.push_back({.SkinIndex = node.Skin, .MeshIndex = node.Mesh, .PrimitiveIndex = i});
                    mesh.PrimitivesSkinIndices[i] = skinIndex;
                }
                totalSkinsCount += primitiveCount;
            }
        }
        
        return true;
    };

    auto readAccessor = [&]<typename T>(const assetlib::GeometryBufferAsset& buffer,
        const assetlib::GeometryBufferAccessor& accessor, std::vector<T>& destinationVector) -> u32 {
            
        const u32 fistElement = (u32)destinationVector.size();
        auto& bufferHeader = buffer.Header;

        if (accessor.Sparse.has_value())
        {
            auto& sparse = *accessor.Sparse;
            ASSERT(sparse.Indices.ComponentType == assetlib::GeometryBufferAccessorComponentType::U32)
            
            const auto& indicesView = bufferHeader.BufferViews[sparse.Indices.BufferView];
            const auto& dataView = bufferHeader.BufferViews[sparse.Data.BufferView];

            const Span indices(
                (const u32*)(buffer.Data.data() + indicesView.OffsetBytes + sparse.Indices.OffsetBytes), sparse.Count);
            const Span data(
                (const T*)(buffer.Data.data() + dataView.OffsetBytes + sparse.Data.OffsetBytes), sparse.Count);

            std::vector<u32> sparseIndices;
            std::vector<T> sparseData;
            sparseIndices.append_range(indices);
            sparseData.append_range(data);
            
            destinationVector.insert(destinationVector.end(), accessor.Count, T{});
            for (auto&& [denseIndex, sparseIndex] : std::views::enumerate(sparseIndices))
                destinationVector[fistElement + sparseIndex] = sparseData[denseIndex];
            
            return fistElement;
        }
    
        const auto& view = bufferHeader.BufferViews[accessor.BufferView];
        const Span<const T> data(
            (const T*)(buffer.Data.data() + view.OffsetBytes + accessor.OffsetBytes), 
            accessor.Count);
    
        destinationVector.append_range(data);
            
        return fistElement;
    };
    auto readAttributeAccessor = [&]<typename T>(const assetlib::GeometryBufferAsset& buffer,
        const assetlib::MeshAttribute* attribute, std::vector<T>& destinationVector) -> u32 {
        if (attribute == nullptr)
            return SceneRenderObject::INVALID;
        
        return readAccessor(buffer, buffer.Header.Accessors[attribute->Accessor], destinationVector);
    };
    
    if (!preload())
        return false;
    
    geometry.MeshRenderObjects.reserve(importedMeshes.size());
    geometry.RenderObjects.reserve(totalRenderObjectCount);
    geometry.BlendShapes.reserve(totalBlendShapeCount);
    for (auto&& [buffer, mesh, primitiveSkinIndices] : importedMeshes)
    {
        geometry.MeshRenderObjects.push_back({
            .FirstRenderObject = (u32)geometry.RenderObjects.size(),
            .RenderObjectCount = (u32)mesh.Primitives.size()
        });
        
        auto& accessors = buffer->Header.Accessors;
        for (auto&& [primitiveIndex, primitive] : std::views::enumerate(mesh.Primitives))
        {
            const auto* positionAttribute = primitive.FindAttribute(assetlib::MeshAttribute::POSITION_NAME);
            const u32 vertexCount = accessors[positionAttribute->Accessor].Count;
            const u32 firstIndex = readAccessor(*buffer, accessors[primitive.IndicesAccessor], geometry.Indices);
            const u32 firstPosition = readAttributeAccessor(*buffer, positionAttribute, geometry.Positions); 
            const u32 firstNormal = readAttributeAccessor(*buffer,
                primitive.FindAttribute(assetlib::MeshAttribute::NORMAL_NAME),
                geometry.Normals); 
            const u32 firstTangent = readAttributeAccessor(*buffer,
                primitive.FindAttribute(assetlib::MeshAttribute::TANGENT_NAME),
                geometry.Tangents); 
            const u32 firstUv = readAttributeAccessor(*buffer,
                primitive.FindAttribute(assetlib::MeshAttribute::UV0_NAME),
                geometry.UVs); 
            const auto* meshletAttribute = primitive.FindAttribute(assetlib::MeshAttribute::MESHLET_NAME);
            const u32 firstMeshlet = readAttributeAccessor(*buffer, meshletAttribute, geometry.Meshlets); 
            const u32 meshletCount = accessors[meshletAttribute->Accessor].Count;
            
            const u32 blendShapeCount = (u32)primitive.BlendShapes.size();
            const u32 firstBlendShape = blendShapeCount == 0 ? SceneRenderObject::INVALID : 
                (u32)geometry.BlendShapes.size();
            
            for (auto& blendShape : primitive.BlendShapes)
            {
                const u32 firstBlendPosition = readAttributeAccessor(*buffer,
                    blendShape.FindAttribute(assetlib::MeshAttribute::POSITION_NAME),
                    geometry.Positions); 
                const u32 firstBlendNormal = readAttributeAccessor(*buffer,
                    blendShape.FindAttribute(assetlib::MeshAttribute::NORMAL_NAME),
                    geometry.Normals); 
                const u32 firstBlendTangent = readAttributeAccessor(*buffer,
                    blendShape.FindAttribute(assetlib::MeshAttribute::TANGENT_NAME),
                    geometry.Tangents);
                
                geometry.BlendShapes.push_back({
                    .Name = blendShape.Name,
                    .Weight = blendShape.Weight,
                    .FirstPosition = firstBlendPosition,
                    .FirstNormal = firstBlendNormal,
                    .FirstTangent = firstBlendTangent,
                });
            }

            const u32 skinIndex = primitiveSkinIndices.empty() ?
                SceneRenderObject::INVALID :
                primitiveSkinIndices[primitiveIndex]; 
            geometry.RenderObjects.push_back({
                .Material = getMaterialIndex(primitive.Material),
                .FirstIndex = firstIndex,
                .FirstPosition = firstPosition,
                .FirstNormal = firstNormal,
                .FirstTangent = firstTangent,
                .FirstUv = firstUv,
                .VertexCount = vertexCount,
                .FirstMeshlet = firstMeshlet,
                .MeshletCount = meshletCount,
                .SkinIndex = skinIndex,
                .BlendShapeIndex = firstBlendShape,
                .BlendShapeCount = blendShapeCount,
                .BoundingBox = primitive.BoundingBox,
                .BoundingSphere = primitive.BoundingSphere,
            });
        }
    }
    
    geometry.SkinJoints.reserve(scene.Skins.size());
    for (auto& skin : scene.Skins)
        geometry.SkinJoints.push_back({.JointNodes = skin.JointNodes});
    
    geometry.Skins.reserve(totalSkinsCount);
    for (auto&& [skinIndex, meshIndex, primitiveIndex] : skins)
    {
        auto& skinInfo = importedSkins[skinIndex];
        
        auto& skin = scene.Skins[skinIndex];
        auto& meshInfo = importedMeshes[meshIndex];
        auto& primitive = importedMeshes[meshIndex].MeshAsset.Primitives[primitiveIndex];
            
        const assetlib::GeometryBufferAsset* skinBuffer = skinInfo.Buffer;
        const assetlib::GeometryBufferAsset* meshBuffer = meshInfo.Buffer;

        if (!skinInfo.IsLoaded())
        {
            auto& skinAccessors = skinBuffer->Header.Accessors;
            skinInfo.FirstJointMatrix = readAccessor(*skinBuffer, skinAccessors[skin.InverseBindMatrixAccessor],
                geometry.JointInverseBindMatrices);
        }
        
        geometry.Skins.push_back({
            .FirstJointMatrix = skinInfo.FirstJointMatrix,
            .FirstJoint = readAttributeAccessor(*meshBuffer,
                primitive.FindAttribute(assetlib::MeshAttribute::JOINTS0_NAME), geometry.Joints),
            .FirstWeight = readAttributeAccessor(*meshBuffer,
                primitive.FindAttribute(assetlib::MeshAttribute::WEIGHTS0_NAME), geometry.Weights)
        });
    }
    
    return true;
}

std::optional<assetlib::GeometryBufferAsset> SceneAssetManager::LoadGeometryBuffer(assetlib::AssetId buffer)
{
    using enum assetlib::GeometryBufferViewType;
    
    auto* geometryBufferAssetInfo = m_AssetSystem->Resolve(buffer);
    if (!geometryBufferAssetInfo)
        return std::nullopt;
    
    import::GeometryBufferImporter importer(m_Ctx, {});
    auto imported = importer.Import(geometryBufferAssetInfo->MetaPath);
    if (!imported.has_value())
        return std::nullopt;
    
    return importer.GetImportedBuffer().Asset;
}

assetlib::GeometryBufferAsset* SceneAssetManager::GetGeometryBuffer(assetlib::AssetId buffer, 
    std::unordered_map<assetlib::AssetId, assetlib::GeometryBufferAsset>& importedBuffers)
{
    if (!importedBuffers.contains(buffer))
    {
        auto importedBuffer = LoadGeometryBuffer(buffer);
        if (!importedBuffer.has_value())
            return nullptr;

        importedBuffers.emplace(buffer, std::move(*importedBuffer));
    }

    return &importedBuffers.at(buffer);
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
        .MetallicRoughnessTexture = LoadTexture(
            materialInfo.MetallicRoughnessUvIndex, materialAsset.MetallicRoughnessTexture,
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
