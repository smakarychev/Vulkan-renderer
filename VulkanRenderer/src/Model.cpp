﻿#include "Model.h"

#include "AssetLib.h"
#include "Core/core.h"
#include "Mesh.h"
#include "ModelAsset.h"
#include "Scene.h"

Model Model::LoadFromAsset(std::string_view path)
{
    Model model = {};
    model.m_ModelName = path;
    
    assetLib::File modelFile;
    assetLib::loadAssetFile(path, modelFile);
    assetLib::ModelInfo modelInfo = assetLib::readModelInfo(modelFile);
    ASSERT(modelInfo.VertexFormat == assetLib::VertexFormat::P3N3C3UV2, "Unsupported vertex format")

    std::vector<VertexP3N3UV> vertices(modelInfo.VerticesSizeBytes());
    std::vector<u32> indices(modelInfo.IndicesSizeBytes());

    assetLib::unpackModel(modelInfo, modelFile.Blob.data(), modelFile.Blob.size(), (u8*)vertices.data(), (u8*)indices.data());

    u32 verticesOffset = 0;
    u32 indicesOffset = 0;
    
    std::vector<MeshInfo> meshes;
    meshes.reserve(modelInfo.MeshInfos.size());
    for (auto& meshInfo : modelInfo.MeshInfos)
    {
        auto verticesBegin = vertices.begin() + verticesOffset;
        verticesOffset += (u32)(meshInfo.VerticesSizeBytes / sizeof(VertexP3N3UV));
        auto verticesEnd = vertices.begin() + verticesOffset;

        auto indicesBegin = indices.begin() + indicesOffset;
        indicesOffset += (u32)(meshInfo.IndicesSizeBytes / sizeof(u32));
        auto indicesEnd = indices.begin() + indicesOffset;

        assetLib::ModelInfo::MaterialInfo& albedo = meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialType::Albedo];
        MaterialInfo albedoMaterial = {.Color = albedo.Color, .Textures = albedo.Textures};
        
        meshes.push_back({Mesh(std::vector(verticesBegin, verticesEnd), std::vector(indicesBegin, indicesEnd)), albedoMaterial});
    }

    model.m_Meshes = meshes;
    
    return model; 
}

void Model::Upload(const Renderer& renderer)
{
    for (auto& mesh : m_Meshes)
        mesh.Mesh.Upload(renderer);
}

void Model::CreateRenderObjects(Scene* scene, const glm::mat4& transform)
{
    for (u32 i = 0; i < m_Meshes.size(); i++)
    {
        auto& mesh = m_Meshes[i];
        std::string textureName = "texture_" + m_ModelName + std::to_string(i);
        std::string meshName = "mesh_" + m_ModelName + std::to_string(i);
        std::string materialName = "mat_" + m_ModelName + std::to_string(i);

        if (scene->GetMaterial(materialName) == nullptr)
        {
            Material material;
            material.Albedo = mesh.Albedo.Color;
            if (!mesh.Albedo.Textures.empty())
            {
                material.AlbedoTexture = textureName;

                if (scene->GetTexture(textureName) == nullptr)
                {
                    Image texture = Image::Builder()
                        .FromAssetFile(mesh.Albedo.Textures.front())
                        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                        .CreateMipmaps(true)
                        .Build();
                    scene->AddTexture(texture, textureName);
                }
            }
            scene->AddMaterial(material, materialName);
            scene->AddMaterialBindless({.Albedo = material.Albedo}, materialName);
        }

        if (scene->GetMesh(meshName) == nullptr)
            scene->AddMesh(mesh.Mesh, meshName);
        
        scene->AddRenderObject({.Mesh = scene->GetMesh(meshName),
            .Material = scene->GetMaterial(materialName),
            .MaterialBindless = scene->GetMaterialBindless(materialName),
            .Transform = transform});
    }
}