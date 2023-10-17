#include "Model.h"

#include "AssetLib.h"
#include "AssetManager.h"
#include "Core/core.h"
#include "Mesh.h"
#include "ModelAsset.h"
#include "Scene.h"

Model* Model::LoadFromAsset(std::string_view path)
{
    Model* cachedModel = AssetManager::GetModel(std::string{path});
    if (cachedModel)
        return cachedModel;
    
    Model model = {};
    model.m_ModelName = path;
    
    assetLib::File modelFile;
    assetLib::loadAssetFile(path, modelFile);
    assetLib::ModelInfo modelInfo = assetLib::readModelInfo(modelFile);
    ASSERT(modelInfo.VertexFormat == assetLib::VertexFormat::P3N3UV2, "Unsupported vertex format")

    std::vector<u64> vertexElementsSizeBytes = modelInfo.VertexElementsSizeBytes();
    std::vector<glm::vec3> positions(vertexElementsSizeBytes[(u32)assetLib::VertexElement::Position]);
    std::vector<glm::vec3> normals(vertexElementsSizeBytes[(u32)assetLib::VertexElement::Normal]);
    std::vector<glm::vec2> uvs(vertexElementsSizeBytes[(u32)assetLib::VertexElement::UV]);
    std::vector<u32> indices(modelInfo.IndicesSizeBytes());

    assetLib::unpackModel(modelInfo, modelFile.Blob.data(), modelFile.Blob.size(),
        {(u8*)positions.data(), (u8*)normals.data(), (u8*)uvs.data()}, (u8*)indices.data());

    u32 positionsOffset = 0;
    u32 normalsOffset = 0;
    u32 uvsOffset = 0;
    u32 indicesOffset = 0;
    
    std::vector<MeshInfo> meshes;
    meshes.reserve(modelInfo.MeshInfos.size());
    for (auto& meshInfo : modelInfo.MeshInfos)
    {
        auto positionsBegin = positions.begin() + positionsOffset;
        positionsOffset += (u32)(meshInfo.VertexElementsSizeBytes[(u32)assetLib::VertexElement::Position] / sizeof(glm::vec3));
        auto positionsEnd = positions.begin() + positionsOffset;

        auto normalsBegin = normals.begin() + normalsOffset;
        normalsOffset += (u32)(meshInfo.VertexElementsSizeBytes[(u32)assetLib::VertexElement::Normal] / sizeof(glm::vec3));
        auto normalsEnd = normals.begin() + normalsOffset;

        auto uvsBegin = uvs.begin() + uvsOffset;
        uvsOffset += (u32)(meshInfo.VertexElementsSizeBytes[(u32)assetLib::VertexElement::UV] / sizeof(glm::vec2));
        auto uvsEnd = uvs.begin() + uvsOffset;

        auto indicesBegin = indices.begin() + indicesOffset;
        indicesOffset += (u32)(meshInfo.IndicesSizeBytes / sizeof(u32));
        auto indicesEnd = indices.begin() + indicesOffset;

        assetLib::ModelInfo::MaterialInfo& albedo = meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialType::Albedo];
        MaterialInfo albedoMaterial = {.Color = albedo.Color, .Textures = albedo.Textures};
        
        meshes.push_back(
            {Mesh(
                std::vector(positionsBegin, positionsEnd),
                std::vector(normalsBegin, normalsEnd),
                std::vector(uvsBegin, uvsEnd),
                std::vector(indicesBegin, indicesEnd)),
                albedoMaterial});
    }

    model.m_Meshes = meshes;

    AssetManager::AddModel(std::string{path}, model);
    return AssetManager::GetModel(std::string{path}); 
}

void Model::Upload(ResourceUploader& uploader)
{
    for (auto& mesh : m_Meshes)
        mesh.Mesh.Upload(uploader);
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
            scene->AddMaterialGPU({.Albedo = material.Albedo}, materialName);
        }

        if (scene->GetMesh(meshName) == nullptr)
            scene->AddMesh(mesh.Mesh, meshName);
        
        scene->AddRenderObject({.Mesh = scene->GetMesh(meshName),
            .Material = scene->GetMaterial(materialName),
            .MaterialBindless = scene->GetMaterialBindless(materialName),
            .Transform = transform});
    }
}
