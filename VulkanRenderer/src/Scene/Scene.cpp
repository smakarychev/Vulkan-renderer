#include "Scene.h"

#include "AssetManager.h"
#include "SceneAsset.h"
#include "Vulkan/Device.h"

SceneInfo* SceneInfo::LoadFromAsset(std::string_view assetPath,
    BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue)
{
    if (SceneInfo* cached = AssetManager::GetSceneInfo(assetPath))
        return cached;
    
    SceneInfo scene = {};

    assetLib::SceneInfo sceneInfo = *assetLib::readSceneHeader(assetPath);
    assetLib::readSceneBinary(sceneInfo);

    scene.m_Geometry = SceneGeometryInfo::FromAsset(sceneInfo, texturesRingBuffer, deletionQueue);
    scene.m_Lights = SceneLightInfo::FromAsset(sceneInfo);
    scene.m_Hierarchy = SceneHierarchyInfo::FromAsset(sceneInfo);

    return AssetManager::AddSceneInfo(assetPath, std::move(scene));
}

Scene Scene::CreateEmpty(DeletionQueue& deletionQueue)
{
    Scene scene = {};

    scene.m_Geometry = SceneGeometry2::CreateEmpty(deletionQueue);
    scene.m_Lights = SceneLight2::CreateEmpty(deletionQueue);

    return scene;
}

SceneInstance Scene::Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
    FrameContext& ctx)
{
    const SceneInstance instance = RegisterSceneInstance(sceneInfo);
    if (m_SceneInstancesMap[&sceneInfo] == 1)
        m_Geometry.Add(instance, ctx);
    SceneGeometry2::AddCommandsResult addCommandsResult = m_Geometry.AddCommands(instance, ctx);
    m_Lights.Add(instance);
    m_Hierarchy.Add(instance, instantiationData.Transform);

    m_InstanceAddedSignal.Emit({
        .SceneInfo = &sceneInfo,
        .RenderObjectsOffset = addCommandsResult.FirstRenderObject});
    
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
