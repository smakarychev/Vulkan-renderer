﻿#pragma once

#include <vector>

#include "RenderHandleArray.h"
#include "Mesh.h"
#include "RenderObject.h"

class Scene;
class ShaderPipelineTemplate;
class Renderer;

// todo: remove once Scene is ready

class Model
{
    friend class ModelCollection;
    struct MeshInfo
    {
        Mesh Mesh;
        Material Material;
    };
public:
    static Model* LoadFromAsset(std::string_view path);
private:
    std::vector<MeshInfo> m_Meshes;
    std::string m_ModelName;
};