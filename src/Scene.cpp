#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"

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

void Scene::AddMaterial(const Material& material, const std::string& name)
{
    m_Materials[name] = material;
}

void Scene::AddMesh(const Mesh& mesh, const std::string& name)
{
    m_Meshes.emplace(std::make_pair(name, mesh));
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}
