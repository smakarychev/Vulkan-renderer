#include "Model.h"

#include "AssetLib.h"
#include "core.h"
#include "Mesh.h"
#include "ModelAsset.h"
#include "Scene.h"

Model Model::LoadFromAsset(std::string_view path)
{
    Model model = {};
    model.m_ModelName = path;
    
    assetLib::File modelFile;
    assetLib::loadBinaryFile(path, modelFile);
    assetLib::ModelInfo modelInfo = assetLib::readModelInfo(modelFile);
    ASSERT(modelInfo.VertexFormat == assetLib::VertexFormat::P3N3C3UV2, "Unsupported vertex format")

    std::vector<Vertex3D> vertices(modelInfo.VerticesSizeBytes());
    std::vector<u32> indices(modelInfo.IndicesSizeBytes());

    assetLib::unpackModel(modelInfo, modelFile.Blob.data(), modelFile.Blob.size(), (u8*)vertices.data(), (u8*)indices.data());

    u32 verticesOffset = 0;
    u32 indicesOffset = 0;
    
    std::vector<MeshInfo> meshes;
    meshes.reserve(modelInfo.MeshInfos.size());
    for (auto& meshInfo : modelInfo.MeshInfos)
    {
        auto verticesBegin = vertices.begin() + verticesOffset;
        verticesOffset += (u32)(meshInfo.VerticesSizeBytes / sizeof(Vertex3D));
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

void Model::CreateRenderObjects(Scene* scene, const RenderPass& renderPass, const glm::mat4& transform, const std::array<Buffer, BUFFERED_FRAMES>& materialBuffer)
{
    ShaderDescriptorSet::Builder texturedDescriptor = ShaderDescriptorSet::Builder()
        .SetTemplate(scene->GetShaderTemplate("textured"));

    ShaderDescriptorSet::Builder defaultDescriptor = ShaderDescriptorSet::Builder()
        .SetTemplate(scene->GetShaderTemplate("default"));

    ShaderPipeline texturedPipeline = ShaderPipeline::Builder()
        .SetTemplate(scene->GetShaderTemplate("textured"))
        .CompatibleWithVertex(Vertex3D::GetInputDescription())
        .SetRenderPass(renderPass)
        .Build();

    ShaderPipeline defaultPipeline = ShaderPipeline::Builder()
        .SetTemplate(scene->GetShaderTemplate("default"))
        .CompatibleWithVertex(Vertex3D::GetInputDescription())
        .SetRenderPass(renderPass)
        .Build();

    
    for (u32 i = 0; i < m_Meshes.size(); i++)
    {
        auto& mesh = m_Meshes[i];
        std::string textureName = "texture_" + m_ModelName + std::to_string(i);
        std::string meshName = "mesh_" + m_ModelName + std::to_string(i);
        std::string materialName = "mat_" + m_ModelName + std::to_string(i);
        
        Material material;
        material.Albedo = mesh.Albedo.Color;
        if (mesh.Albedo.Textures.empty())
        {
            material.Pipeline = defaultPipeline;
            for (u32 j = 0; j < material.DescriptorSets.size(); j++)
            {
                material.DescriptorSets[j] = defaultDescriptor
                    .AddBinding("u_material_buffer", materialBuffer[j])
                    .Build(); 
            }
        }
        else
        {
            material.Pipeline = texturedPipeline;
            if (scene->GetTexture(textureName) == nullptr)
            {
                Image texture = Image::Builder()
                    .FromAssetFile(mesh.Albedo.Textures.front())
                    .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                    .CreateMipmaps(true)
                    .Build();
                scene->AddTexture(texture, textureName);
            }
            
            for (u32 j = 0; j < material.DescriptorSets.size(); j++)
            {
                material.DescriptorSets[j] = texturedDescriptor
                    .AddBinding("u_material_buffer", materialBuffer[j])
                    .AddBinding("u_texture", *scene->GetTexture(textureName))
                    .Build(); 
            }
        }

        if (scene->GetMaterial(materialName) == nullptr)
            scene->AddMaterial(material, materialName);

        if (scene->GetMesh(meshName) == nullptr)
            scene->AddMesh(mesh.Mesh, meshName);
        
        scene->AddRenderObject({.Mesh = scene->GetMesh(meshName), .Material = scene->GetMaterial(materialName), .Transform = transform});
    }
}
