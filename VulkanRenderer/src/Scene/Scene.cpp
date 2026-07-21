#include "rendererpch.h"

#include "Scene.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Assets/Materials/MaterialAssetManager.h"

Scene::Scene(DeletionQueue& deletionQueue, lux::SceneAssetManager& sceneAssetManager)
    : m_SceneAssetManager(&sceneAssetManager)
{
    using SceneDeletedInfo = lux::SceneAssetManager::SceneDeletedInfo;
    using SceneReplacedInfo = lux::SceneAssetManager::SceneReplacedInfo;
    using MaterialUpdatedInfo = lux::SceneAssetManager::MaterialUpdatedInfo;
    m_Geometry = SceneGeometry::CreateEmpty(deletionQueue);
    m_Lights = SceneLight::CreateEmpty(deletionQueue);
    m_SceneDeletedHandler = SignalHandler<SceneDeletedInfo>([this](const SceneDeletedInfo& sceneDeletedInfo) {
        DeleteScene(sceneDeletedInfo.Scene);
    });
    m_SceneReplacedHandler = SignalHandler<SceneReplacedInfo>([this](const SceneReplacedInfo& sceneReplacedInfo) {
        ReplaceScene(sceneReplacedInfo.Scene, sceneReplacedInfo.Scene);
    });
    m_MaterialUpdatedHandler = SignalHandler<MaterialUpdatedInfo>([this](
        const MaterialUpdatedInfo& materialUpdatedInfo) {
        UpdateMaterial(materialUpdatedInfo.Scene);   
    });
    m_SceneDeletedHandler.Connect(sceneAssetManager.GetSceneDeletedSignal());
    m_SceneReplacedHandler.Connect(sceneAssetManager.GetSceneReplacedSignal());
    m_MaterialUpdatedHandler.Connect(sceneAssetManager.GetMaterialUpdatedSignal());
}


lux::SceneInstanceHandle Scene::Instantiate(lux::SceneHandle scene, const SceneInstantiationData& instantiationData)
{
    const lux::SceneInstanceHandle instance = RegisterSceneInstance(scene);
    InstantiateHandle(instance, instantiationData);
    
    return instance;
}

void Scene::InstantiateHandle(lux::SceneInstanceHandle handle, const SceneInstantiationData& instantiationData)
{
    m_NewInstances.push_back({
        .Instance = handle,
        .InstantiationData = instantiationData
    });
}

void Scene::Delete(lux::SceneInstanceHandle instance)
{
    auto it = std::ranges::find_if(m_NewInstances, [&instance](auto& newInstance) {
        return instance == newInstance.Instance;
    });
    const bool isJustAdded = it != m_NewInstances.end();
    if (isJustAdded)
    {
        m_NewInstances.erase(it);
        return;
    }

    m_InstanceIsAlive[instance] = false;
    m_DeletedInstances.push_back(instance);
}

void Scene::OnUpdate(FrameContext& ctx)
{
    HandleSpawnAndSweep(ctx, /*reclaimHandles*/true);
    HandleReplacements(ctx);
    HandleMaterialUpdates(ctx);
    
    UpdateHierarchy(ctx);
    m_Lights.OnUpdate(ctx);
}

lux::SceneInstanceHandle Scene::RegisterSceneInstance(lux::SceneHandle scene)
{
    lux::SceneInstanceHandle handle = m_ActiveInstances.insert(SceneInstance{});
    MapSceneInstanceToSceneInfo(scene, handle);
    
    return handle;
}

void Scene::MapSceneInstanceToSceneInfo(lux::SceneHandle scene, lux::SceneInstanceHandle handle)
{
    m_ScenesMap[scene].Instances.insert(handle);
    m_ActiveInstances[handle].Scene = scene;
}

Scene::NewInstanceData Scene::AddToHierarchy(lux::SceneInstanceHandle instance, const Transform3d& baseTransform,
    FrameContext& ctx)
{
    const lux::SceneAsset& sceneAsset = *m_SceneAssetManager->Get(m_ActiveInstances[instance].Scene);
    const lux::SceneHierarchyInfo& instanceHierarchy = sceneAsset.Hierarchy; 
    const u32 firstNode = (u32)m_HierarchyInfo.Nodes.size();
    
    const SceneGeometry::AddInstanceResult addResult = m_Geometry.AddInstance(sceneAsset, instance, ctx);
    m_MaxRenderObjectIndex = std::max(
        m_MaxRenderObjectIndex, addResult.FirstRenderObject + (u32)sceneAsset.Geometry.RenderObjects.size());

    for (auto& node : instanceHierarchy.Nodes)
    {
        const bool isTopLevel = node.Parent == lux::SceneHierarchyHandle::INVALID;
        lux::SceneHierarchyPayload payload = {};
        switch (node.Type)
        {
        case lux::SceneHierarchyNodeType::Mesh:
            {
                auto& mesh = node.Payload.Mesh;
                payload.Mesh = {
                    .FirstRenderObject = mesh.FirstRenderObject + addResult.FirstRenderObject,
                    .RenderObjectCount = mesh.RenderObjectCount,
                    .FirstBlendShape = mesh.FirstBlendShape != lux::SceneHierarchyPayload::INVALID ?
                        mesh.FirstBlendShape + addResult.FirstBlendShape :
                        lux::SceneHierarchyPayload::INVALID
                };
            }
            break;
        case lux::SceneHierarchyNodeType::Light:
            payload.Light.Index = m_Lights.Add(sceneAsset.Lights.Lights[node.Payload.Light.Index]);
            break;
        case lux::SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        m_HierarchyInfo.Nodes.push_back({
            .Type = node.Type,
            .Depth = node.Depth,
            .Parent = isTopLevel ? lux::SceneHierarchyHandle::INVALID : node.Parent + firstNode,
            .LocalTransform = isTopLevel ?
                baseTransform.Combine(node.LocalTransform) :
                node.LocalTransform,
            .Payload = payload,
            .Instance = instance
        });
    }
    m_HierarchyInfo.MaxDepth = std::max(m_HierarchyInfo.MaxDepth, instanceHierarchy.MaxDepth);
    
    for (auto& joint : instanceHierarchy.Joints)
        m_HierarchyInfo.Joints.push_back({
            .Node = joint.Node.Handle + firstNode,
            .JointMatrixIndex = addResult.FirstJointMatrix + joint.JointMatrixIndex,
            .InverseBindMatrix = joint.InverseBindMatrix
        });
    auto& animationChannels = m_HierarchyInfo.AnimationChannels;
    for (auto& animation : instanceHierarchy.Animations)
        m_HierarchyInfo.Animations.push_back({
            .Name = animation.Name,
            .Node = animation.Node + firstNode,
            .TranslationChannel = animation.TranslationChannel == lux::SceneHierarchyAnimation::INVALID ?
                animation.TranslationChannel : 
                animationChannels.insert(instanceHierarchy.AnimationChannels[animation.TranslationChannel]),
            .OrientationChannel = animation.OrientationChannel == lux::SceneHierarchyAnimation::INVALID ?
                animation.OrientationChannel : 
                animationChannels.insert(instanceHierarchy.AnimationChannels[animation.OrientationChannel]),
            .ScaleChannel = animation.ScaleChannel == lux::SceneHierarchyAnimation::INVALID ?
                animation.ScaleChannel : 
                animationChannels.insert(instanceHierarchy.AnimationChannels[animation.ScaleChannel]),
            .WeightChannel = animation.WeightChannel == lux::SceneHierarchyAnimation::INVALID ?
                animation.WeightChannel : 
                animationChannels.insert(instanceHierarchy.AnimationChannels[animation.WeightChannel]),
        });
    
    return {
        .Scene = &sceneAsset,
        .Instance = instance,
        .RenderObjectsOffset = addResult.FirstRenderObject,
    };
}

void Scene::DeleteScene(lux::SceneHandle scene)
{
    m_ReplacedScenes.push_back({.Original = scene, .Replacement = {}});
}

void Scene::ReplaceScene(lux::SceneHandle original, lux::SceneHandle replacement)
{
    m_ReplacedScenes.push_back({.Original = original, .Replacement = replacement});
}

void Scene::UpdateMaterial(lux::SceneHandle scene)
{
    m_UpdatedMaterials.push_back({.Scene = scene});
}

void Scene::HandleReplacements(FrameContext& ctx)
{
    auto getTopNodeLevelTransform = [&](const lux::SceneHierarchyNode& node) -> Transform3d {
        lux::SceneHierarchyHandle parent = node.Parent;
        if (parent == lux::SceneHierarchyHandle::INVALID)
            return node.LocalTransform;

        for (;;)
        {
            lux::SceneHierarchyHandle current = parent;
            parent = m_HierarchyInfo.Nodes[parent.Handle].Parent;
            if (parent == lux::SceneHierarchyHandle::INVALID)
                return m_HierarchyInfo.Nodes[current.Handle].LocalTransform;
        }
    };
    
    if (m_ReplacedScenes.empty())
        return;

    struct ReinstantiationData
    {
        lux::SceneHandle Replacement{};
        SceneInstantiationData InstantiationData{};
    };
    std::unordered_map<lux::SceneInstanceHandle, ReinstantiationData> reinstantiationData;
    for (auto&& [original, replacement] : m_ReplacedScenes)
    {
        auto& originalInstances = m_ScenesMap[original].Instances;
        for (auto& node : m_HierarchyInfo.Nodes)
        {
            if (originalInstances.contains(node.Instance) && !reinstantiationData.contains(node.Instance))
            {
                const lux::SceneAsset& originalScene = *m_SceneAssetManager->Get(original);
                const Transform3d topNodeOriginalTransform = originalScene.Hierarchy.Nodes.front().LocalTransform;
                
                reinstantiationData.emplace(
                    node.Instance, ReinstantiationData{
                        .Replacement = replacement,
                        .InstantiationData = {
                            .Transform = getTopNodeLevelTransform(node).Combine(topNodeOriginalTransform.Inverse())
                        }
                    });
            }
        }
    }
    auto nodesBackup = m_HierarchyInfo.Nodes;
    for (const auto& instance : reinstantiationData | std::views::keys)
        Delete(instance);
    Sweep(/*reclaimHandles*/false);

    for (auto&& [original, _] : m_ReplacedScenes)
    {
        m_Geometry.Delete(*m_SceneAssetManager->Get(original));
        m_ScenesMap[original].HasGeometry = false;
    }
    
    for (auto&& [instance, reinstantiationInfo] : reinstantiationData) {
        if (!reinstantiationInfo.Replacement.IsValid())
            continue;
        MapSceneInstanceToSceneInfo(reinstantiationInfo.Replacement, instance);
        InstantiateHandle(instance, reinstantiationInfo.InstantiationData);
    }
    Spawn(ctx);
    
    m_ReplacedScenes.clear();
}

void Scene::HandleSpawnAndSweep(FrameContext& ctx, bool reclaimHandles)
{
    Spawn(ctx);
    Sweep(reclaimHandles);
}

void Scene::Spawn(FrameContext& ctx)
{
    for (auto&& [instanceHandle, instantiationData] : m_NewInstances)
    {
        auto& instance = m_ActiveInstances[instanceHandle];
        if (!m_ScenesMap[instance.Scene].HasGeometry)
        {
            m_Geometry.Add(*m_SceneAssetManager->Get(instance.Scene), ctx);
            m_ScenesMap[instance.Scene].HasGeometry = true;
        }

        if (instanceHandle >= m_InstanceIsAlive.size())
            m_InstanceIsAlive.resize(instanceHandle + 1);
        m_InstanceIsAlive[instanceHandle] = true;
        m_InstanceAddedSignal.Emit(AddToHierarchy(instanceHandle, instantiationData.Transform, ctx));
    }
    m_NewInstances.clear();
}

void Scene::Sweep(bool reclaimHandles)
{
    if (m_DeletedInstances.empty())
        return;
    
    auto reparentToValid = [&](lux::SceneHierarchyNode& node) {
        lux::SceneHierarchyHandle parent = node.Parent;
        while (
            parent != lux::SceneHierarchyHandle::INVALID &&
            !m_InstanceIsAlive[m_HierarchyInfo.Nodes[parent].Instance])
        {
            parent = m_HierarchyInfo.Nodes[parent].Parent;
        }
        node.Parent = parent;
    };
    
    std::vector reorder(m_HierarchyInfo.Nodes.size(), lux::SceneHierarchyHandle::INVALID);
    u32 currentLastAliveIndex = 0;
    for (auto&& [i, node] : std::views::enumerate(m_HierarchyInfo.Nodes))
    {
        if (!m_InstanceIsAlive[node.Instance])
            continue;
        
        reparentToValid(node);
        reorder[i] = currentLastAliveIndex;
        currentLastAliveIndex += 1;
    }
    
    for (i32 i = (i32)m_HierarchyInfo.Joints.size() - 1; i >= 0; i--)
    {
        auto& joint = m_HierarchyInfo.Joints[i];
        
        if (reorder[joint.Node.Handle] == lux::SceneHierarchyHandle::INVALID)
        {
            std::swap(joint, m_HierarchyInfo.Joints.back());
            m_HierarchyInfo.Joints.pop_back();
        }
        else
        {
            joint.Node.Handle = reorder[joint.Node.Handle];
        }
    }
    
    for (i32 i = (i32)m_HierarchyInfo.Animations.size() - 1; i >= 0; i--)
    {
        auto& animation = m_HierarchyInfo.Animations[i];
        
        if (reorder[animation.Node.Handle] == lux::SceneHierarchyHandle::INVALID)
        {
            if (animation.TranslationChannel != lux::SceneHierarchyHandle::INVALID)
                m_HierarchyInfo.AnimationChannels.erase(animation.TranslationChannel);
            if (animation.OrientationChannel != lux::SceneHierarchyHandle::INVALID)
                m_HierarchyInfo.AnimationChannels.erase(animation.OrientationChannel);
            if (animation.ScaleChannel != lux::SceneHierarchyHandle::INVALID)
                m_HierarchyInfo.AnimationChannels.erase(animation.ScaleChannel);
            if (animation.WeightChannel != lux::SceneHierarchyHandle::INVALID)
                m_HierarchyInfo.AnimationChannels.erase(animation.WeightChannel);
            
            std::swap(animation, m_HierarchyInfo.Animations.back());
            m_HierarchyInfo.Animations.pop_back();
        }
        else
        {
            animation.Node.Handle = reorder[animation.Node.Handle];
        }
    }
    
    for (u32 i = 0; i < (u32)m_HierarchyInfo.Nodes.size(); i++)
    {
        auto& node = m_HierarchyInfo.Nodes[i];
        if (reorder[i] == lux::SceneHierarchyHandle::INVALID)
        {
            if (node.Type == lux::SceneHierarchyNodeType::Light)
                m_Lights.Delete(node.Payload.Light.Index);
            continue;
        }
        
        if (node.Parent.Handle != lux::SceneHierarchyHandle::INVALID)
            node.Parent.Handle = reorder[node.Parent.Handle];
        
        if (reorder[i] != i)
            m_HierarchyInfo.Nodes[reorder[i]] = std::move(node);
    }
    
    m_HierarchyInfo.Nodes.resize(currentLastAliveIndex);

    for (auto& instanceHandle : m_DeletedInstances)
    {
        auto& instance = m_ActiveInstances[instanceHandle];
        m_Geometry.DeleteRenderObjects(instanceHandle);
        m_ScenesMap[instance.Scene].Instances.erase(instanceHandle);
        m_InstanceDeletedSignal.Emit({
            .Scene = m_SceneAssetManager->Get(instance.Scene),
            .Instance = instanceHandle,
        });
    }

    for (auto&& [i, isAlive] : std::views::enumerate(m_InstanceIsAlive))
    {
        if (!isAlive && reclaimHandles)
            m_ActiveInstances.erase((u32)i);
        m_InstanceIsAlive[i] = true;
    }

    m_DeletedInstances.clear();
}

namespace
{
void updateRenderObject(Buffer renderObjects, u32 renderObjectIndex,
    const glm::mat4& previousTransform, const glm::mat4& transform, ResourceUploader& uploader)
{
    uploader.UpdateBuffer(
        renderObjects,
        Span<const glm::mat4>{transform, previousTransform},
        renderObjectIndex * sizeof(RenderObjectGPU) + offsetof(RenderObjectGPU, Transform));
}

void updateJointMatrix(Buffer jointMatrices, u32 jointIndex, const glm::mat4& transform, ResourceUploader& uploader)
{
    uploader.UpdateBuffer(
        jointMatrices,
        transform,
        jointIndex * sizeof(glm::mat4));
}

void updateBlendShapeWeight(Buffer blendShapes, u32 blendShapeIndex, f32 weight, ResourceUploader& uploader)
{
    uploader.UpdateBuffer(
        blendShapes,
        weight,
        blendShapeIndex * sizeof(BlendShapeGPU) + offsetof(BlendShapeGPU, Weight));
}

void updateLight(lux::CommonLight& light, const glm::mat4& transform)
{
    switch (light.Type)
    {
    case lux::LightType::Directional:
        light.PositionDirection = glm::normalize(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
        break;
    case lux::LightType::Point:
        light.PositionDirection = transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        break;
    case lux::LightType::Spot:
        ASSERT(false, "Spot light is not implemented")
        break;
    }
}
}

void Scene::UpdateHierarchy(FrameContext& ctx)
{
    UpdateAnimations(ctx);
    UpdateTransforms(ctx);
}

void Scene::UpdateAnimations(FrameContext& ctx)
{
    auto& channels = m_HierarchyInfo.AnimationChannels;
    
    for (auto& animationChannel : channels)
        animationChannel.Tick(ctx.Dt);
    
    for (auto& animation : m_HierarchyInfo.Animations)
    {
        auto& node = m_HierarchyInfo.Nodes[animation.Node.Handle];
        if (animation.TranslationChannel != lux::SceneHierarchyAnimation::INVALID)
            node.LocalTransform.Position = channels[animation.TranslationChannel].GetInterpolated().Translation;
        if (animation.OrientationChannel != lux::SceneHierarchyAnimation::INVALID)
            node.LocalTransform.Orientation = channels[animation.OrientationChannel].GetInterpolated().Orientation;
        if (animation.ScaleChannel != lux::SceneHierarchyAnimation::INVALID)
            node.LocalTransform.Scale = channels[animation.ScaleChannel].GetInterpolated().Scale;
        if (animation.WeightChannel != lux::SceneHierarchyAnimation::INVALID)
        {
            auto& mesh = node.Payload.Mesh;
            const u32 blendShapeCount = channels[animation.WeightChannel].ElementCount();
            for (u32 renderObjectIndex = 0; renderObjectIndex < mesh.RenderObjectCount; renderObjectIndex++)
            {
                const u32 blendShapeOffset = renderObjectIndex * blendShapeCount;
                for (u32 element = 0; element < channels[animation.WeightChannel].ElementCount(); element++)
                {
                    const u32 blendShapeIndex = element + mesh.FirstBlendShape + blendShapeOffset;
                    updateBlendShapeWeight(Geometry().BlendShapes.GetUnderlyingBuffer(),
                        blendShapeIndex, channels[animation.WeightChannel].GetInterpolated(element).Weight,
                        *ctx.ResourceUploader);
                }
            }
        }
    }
}

void Scene::UpdateTransforms(FrameContext& ctx)
{
    auto& nodes = m_HierarchyInfo.Nodes;
    std::vector<glm::mat4> transforms(nodes.size());
    m_RenderObjectPreviousTransforms.resize(m_MaxRenderObjectIndex);

    for (u32 i = 0; i < nodes.size(); i++)
        transforms[i] = nodes[i].Parent == lux::SceneHierarchyHandle::INVALID ?
            nodes[i].LocalTransform.ToMatrix() :
            transforms[nodes[i].Parent.Handle] * nodes[i].LocalTransform.ToMatrix();
    
    std::vector<glm::mat4> skinMatrices(m_HierarchyInfo.Joints.size());
    for (auto& joint : m_HierarchyInfo.Joints)
    {
        const glm::mat4 jointMatrix =  
            transforms[joint.Node.Handle] *
            joint.InverseBindMatrix;
        updateJointMatrix(Geometry().JointMatrices.GetUnderlyingBuffer(), joint.JointMatrixIndex, jointMatrix,
            *ctx.ResourceUploader);
    }

    for (auto&& [i, node] : std::views::enumerate(nodes))
    {
        switch (node.Type)
        {
        case lux::SceneHierarchyNodeType::Mesh:
            {
                auto& mesh = node.Payload.Mesh;
                for (u32 renderObjectIndex = 0; renderObjectIndex < mesh.RenderObjectCount; renderObjectIndex++)
                {
                    const u32 globalIndex = mesh.FirstRenderObject + renderObjectIndex;
                    auto& previousTransform = m_RenderObjectPreviousTransforms[globalIndex];
                    updateRenderObject(Geometry().RenderObjects.GetUnderlyingBuffer(), globalIndex,
                        previousTransform, transforms[i], *ctx.ResourceUploader);
                    m_RenderObjectPreviousTransforms[globalIndex] = transforms[i];
                }
            }
            break;
        case lux::SceneHierarchyNodeType::Light:
            updateLight(Lights().Get(node.Payload.Light.Index), transforms[i]);
            break;
        case lux::SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
    }
}

void Scene::HandleMaterialUpdates(FrameContext& ctx)
{
    if (m_UpdatedMaterials.empty())
        return;
    
    for (auto& updateInfo : m_UpdatedMaterials)
    {
        const lux::SceneHandle sceneHandle = updateInfo.Scene;
        if (!m_ScenesMap.contains(sceneHandle))
            continue;
        
        m_Geometry.UpdateMaterials(*m_SceneAssetManager->Get(sceneHandle), ctx);
    }
    
    m_UpdatedMaterials.clear();
}
