﻿#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"
#include "Model.h"

ShaderPipelineTemplate* Scene::GetShaderTemplate(const std::string& name)
{
    auto it = m_ShaderTemplates.find(name);
    if (it == m_ShaderTemplates.end())
        return nullptr;
    return &it->second;
}

MaterialBindless* Scene::GetMaterialBindless(const std::string& name)
{
    auto it = m_MaterialsBindless.find(name);
    if (it == m_MaterialsBindless.end())
        return nullptr;
    return &it->second;
}

Material* Scene::GetMaterial(const std::string& name)
{
    auto it = m_Materials.find(name);
    if (it == m_Materials.end())
        return nullptr;
    return &it->second;
}

Model* Scene::GetModel(const std::string& name)
{
    auto it = m_Models.find(name);
    if (it == m_Models.end())
        return nullptr;
    return &it->second;
}

Mesh* Scene::GetMesh(const std::string& name)
{
    auto it = m_Meshes.find(name);
    if (it == m_Meshes.end())
        return nullptr;
    return &it->second;
}

Texture* Scene::GetTexture(const std::string& name)
{
    auto it = m_Textures.find(name);
    if (it == m_Textures.end())
        return nullptr;
    return &it->second;
}

void Scene::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name)
{
    m_ShaderTemplates[name] = shaderTemplate;
}

void Scene::AddMaterialBindless(const MaterialBindless& material, const std::string& name)
{
    m_MaterialsBindless[name] = material;
}

void Scene::AddMaterial(const Material& material, const std::string& name)
{
    m_Materials[name] = material;
}

void Scene::AddMesh(const Mesh& mesh, const std::string& name)
{
    m_Meshes.emplace(std::make_pair(name, mesh));
}

void Scene::AddModel(const Model& model, const std::string& name)
{
    m_Models.emplace(std::make_pair(name, model));
}

void Scene::AddTexture(const Texture& texture, const std::string& name)
{
    m_Textures.emplace(std::make_pair(name, texture));
}

void Scene::UpdateRenderObject(ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState)
{
    for (u32 i = m_NewRenderObjectsIndex; i < m_RenderObjects.size(); i++)
    {
        RenderObject& object = m_RenderObjects[i];
        Material* material = object.Material;
        MaterialBindless* materialBindless = object.MaterialBindless;
        
        if (material->AlbedoTexture.length() > 0 && materialBindless->AlbedoTextureIndex == MaterialBindless::NO_TEXTURE)
        {
            materialBindless->AlbedoTextureIndex = bindlessDescriptorsState.TextureIndex;
            bindlessDescriptorSet.SetTexture("u_textures", *GetTexture(material->AlbedoTexture), materialBindless->AlbedoTextureIndex);
            bindlessDescriptorsState.TextureIndex++;
        }
    }
}

void Scene::CreateIndirectBatches()
{
    if (m_RenderObjects.empty())
        return;
    
    m_IndirectBatches.clear();

    m_IndirectBatches.push_back(BatchIndirect{
        .Mesh = m_RenderObjects.front().Mesh,
        .MaterialBindless = m_RenderObjects.front().MaterialBindless,
        .First = 0,
        .Count = 0});

    for (auto& object : m_RenderObjects)
    {
        auto& batch = m_IndirectBatches.back(); 
        if (object.Mesh == batch.Mesh)
            batch.Count++;
        else
            m_IndirectBatches.push_back({.Mesh = object.Mesh, .MaterialBindless = object.MaterialBindless, .First = batch.First + batch.Count, .Count = 1});
    }
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    if (!m_IsDirty)
        m_NewRenderObjectsIndex = (u32)m_RenderObjects.size();
    
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}