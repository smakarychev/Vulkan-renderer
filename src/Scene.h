#pragma once
#include <string>
#include <unordered_map>

#include "RenderObject.h"
#include "Vulkan/Image.h"

class RenderObject;
class Mesh;
struct Material;

class Scene
{
public:
    Material* GetMaterial(const std::string& name);
    Mesh* GetMesh(const std::string& name);

    void AddMaterial(const Material& material, const std::string& name);
    void AddMesh(const Mesh& mesh, const std::string& name);
    void AddTexture(const Texture& texture, const std::string& name);

    const std::vector<RenderObject>& GetRenderObjects() const { return m_RenderObjects; }
    std::vector<RenderObject>& GetRenderObjects() { return m_RenderObjects; }

    void AddRenderObject(const RenderObject& renderObject);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
private:
    std::unordered_map<std::string, Material> m_Materials;
    std::unordered_map<std::string, Mesh> m_Meshes;
    std::unordered_map<std::string, Texture> m_Textures;

    std::vector<RenderObject> m_RenderObjects;
    bool m_IsDirty{false};
};
