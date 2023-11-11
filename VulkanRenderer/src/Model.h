#pragma once
#include <vector>

#include "Mesh.h"
#include "ModelAsset.h"
#include "RenderObject.h"

class Scene;
class ShaderPipelineTemplate;
class Renderer;

class Model
{
    struct MaterialInfo
    {
        glm::vec4 Color;
        std::vector<std::string> Textures;
    };
    struct MeshInfo
    {
        Mesh Mesh;
        MaterialInfo Albedo;
    };
public:
    static Model* LoadFromAsset(std::string_view path);
    void CreateRenderObjects(Scene* scene, const glm::mat4& transform,
        ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState);

    void CreateDebugBoundingSpheres(Scene* scene, const glm::mat4& transform,
        ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState);
private:
    std::vector<MeshInfo> m_Meshes;
    std::string m_ModelName;

    bool m_IsInScene{false};
    std::vector<RenderObject> m_RenderObjects;
};
