#pragma once
#include <string_view>
#include <vector>

class Renderer;
class Mesh;

class Model
{
public:
    static Model LoadFromAsset(std::string_view path);
    void Upload(const Renderer& renderer);
    const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }
private:
    std::vector<Mesh> m_Meshes;
};
