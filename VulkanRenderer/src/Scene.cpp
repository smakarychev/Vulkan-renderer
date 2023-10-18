#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"
#include "Model.h"
#include "Vulkan/Shader.h"

ShaderPipelineTemplate* Scene::GetShaderTemplate(const std::string& name)
{
    auto it = m_ShaderTemplates.find(name);
    return it == m_ShaderTemplates.end() ? nullptr : &it->second;
}

Model* Scene::GetModel(const std::string& name)
{
    auto it = m_Models.find(name);
    return it == m_Models.end() ? nullptr : it->second;
}

MaterialGPU& Scene::GetMaterialGPU(RenderHandle<MaterialGPU> handle)
{
    return m_MaterialsGPU[handle];
}

Mesh& Scene::GetMesh(RenderHandle<Mesh> handle)
{
    return m_Meshes[handle];
}

void Scene::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name)
{
    m_ShaderTemplates.emplace(std::make_pair(name, shaderTemplate));
}

void Scene::AddModel(Model* model, const std::string& name)
{
    m_Models.emplace(std::make_pair(name, model));
}

RenderHandle<MaterialGPU> Scene::AddMaterialGPU(const MaterialGPU& material)
{
    RenderHandle<MaterialGPU> handle = (u32)m_MaterialsGPU.size();
    m_MaterialsGPU.push_back(material);
    return handle;
}

RenderHandle<Material> Scene::AddMaterial(const Material& material)
{
    RenderHandle<Material> handle = (u32)m_Materials.size();
    m_Materials.push_back(material);
    return handle;
}

RenderHandle<Mesh> Scene::AddMesh(const Mesh& mesh)
{
    RenderHandle<Mesh> handle = (u32)m_Meshes.size();
    m_Meshes.push_back(mesh);
    return handle;
}

RenderHandle<Texture> Scene::AddTexture(const Texture& texture)
{
    RenderHandle<Texture> handle = (u32)m_Textures.size();
    m_Textures.push_back(texture);
    return handle;
}

void Scene::SetMaterialTexture(MaterialGPU& material, const Texture& texture,
    ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState)
{
    RenderHandle<Texture> albedoHandle = AddTexture(texture);
    Texture& albedoTexture = m_Textures[albedoHandle];
    material.AlbedoTextureHandle = bindlessDescriptorsState.TextureIndex;
    bindlessDescriptorSet.SetTexture("u_textures", albedoTexture, bindlessDescriptorsState.TextureIndex);
    bindlessDescriptorsState.TextureIndex++;
}

void Scene::CreateIndirectBatches()
{
    if (m_RenderObjects.empty())
        return;
    
    m_IndirectBatches.clear();

    m_IndirectBatches.push_back(BatchIndirect{
        .Mesh = m_RenderObjects.front().Mesh,
        .MaterialGPU = m_RenderObjects.front().MaterialGPU,
        .First = 0,
        .InstanceCount = 0});

    for (auto& object : m_RenderObjects)
    {
        auto& batch = m_IndirectBatches.back(); 
        if (object.Mesh == batch.Mesh)
            batch.InstanceCount++;
        else
            m_IndirectBatches.push_back({.Mesh = object.Mesh, .MaterialGPU = object.MaterialGPU, .First = batch.First + batch.InstanceCount, .InstanceCount = 1});
    }
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}
