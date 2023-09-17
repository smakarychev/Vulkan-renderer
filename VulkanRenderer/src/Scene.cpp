#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"

ShaderPipelineTemplate* Scene::GetShaderTemplate(const std::string& name)
{
    auto it = m_ShaderTemplates.find(name);
    if (it == m_ShaderTemplates.end())
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

void Scene::AddMaterial(const Material& material, const std::string& name)
{
    m_Materials[name] = material;
}

void Scene::AddMesh(const Mesh& mesh, const std::string& name)
{
    m_Meshes.emplace(std::make_pair(name, mesh));
}

void Scene::AddTexture(const Texture& texture, const std::string& name)
{
    m_Textures.emplace(std::make_pair(name, texture));
}

void Scene::CreateIndirectBatches()
{
    if (m_RenderObjects.empty())
        return;
    
    m_IndirectBatches.clear();

    m_IndirectBatches.push_back({
        .Mesh = m_RenderObjects.front().Mesh,
        .Material = m_RenderObjects.front().Material,
        .First = 0,
        .Count = 0});
    
    for (auto& object : m_RenderObjects)
    {
        auto& batch = m_IndirectBatches.back(); 
        if (object.Mesh == batch.Mesh && object.Material == batch.Material)
            batch.Count++;
        else
            m_IndirectBatches.push_back({.Mesh = object.Mesh, .Material = object.Material, .First = batch.First + batch.Count, .Count = 1});
    }
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}
