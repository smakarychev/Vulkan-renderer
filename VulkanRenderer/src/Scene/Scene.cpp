#include "rendererpch.h"

#include "Scene.h"

#include "AssetManager.h"
#include "Vulkan/Device.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>

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

Scene Scene::CreateEmpty(DeletionQueue& deletionQueue)
{
    Scene scene = {};

    scene.m_Geometry = SceneGeometry::CreateEmpty(deletionQueue);
    scene.m_Lights = SceneLight::CreateEmpty(deletionQueue);

    return scene;
}

SceneInstance Scene::Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
    FrameContext& ctx)
{
    const SceneInstance instance = RegisterSceneInstance(sceneInfo);
    if (m_SceneInstancesMap[&sceneInfo] == 1)
        m_Geometry.Add(instance, ctx);
    SceneGeometry::AddCommandsResult addCommandsResult = m_Geometry.AddCommands(instance, ctx);
    m_Lights.Add(instance);
    m_Hierarchy.Add(instance, instantiationData.Transform);

    m_InstanceAddedSignal.Emit({
        .SceneInfo = &sceneInfo,
        .RenderObjectsOffset = addCommandsResult.FirstRenderObject,
        .MeshletsOffset = addCommandsResult.FirstMeshlet});
    
    return instance;
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
