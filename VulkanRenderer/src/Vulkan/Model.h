#pragma once
#include <string_view>
#include <vector>

#include "Mesh.h"
#include "Settings.h"

class RenderPass;
class Scene;
class ShaderPipelineTemplate;
class RenderObject;
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
    static Model LoadFromAsset(std::string_view path);
    void Upload(const Renderer& renderer);
    void CreateRenderObjects(Scene* scene, const RenderPass& renderPass, const glm::mat4& transform, const std::array<Buffer, BUFFERED_FRAMES>& materialBuffer);
private:
    std::vector<MeshInfo> m_Meshes;
    std::string m_ModelName;
};
