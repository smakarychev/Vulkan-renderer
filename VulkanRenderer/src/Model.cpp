#include "Model.h"

#include <glm/ext/matrix_transform.hpp>

#include "AssetManager.h"
#include "Core/core.h"
#include "Mesh.h"
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
    ASSERT(modelInfo.VertexFormat == assetLib::VertexFormat::P3N3T3UV2, "Unsupported vertex format")

    std::vector<u64> vertexElementsSizeBytes = modelInfo.VertexElementsSizeBytes();
    std::vector<glm::vec3> positions(vertexElementsSizeBytes[(u32)assetLib::VertexElement::Position]);
    std::vector<glm::vec3> normals(vertexElementsSizeBytes[(u32)assetLib::VertexElement::Normal]);
    std::vector<glm::vec3> tangents(vertexElementsSizeBytes[(u32)assetLib::VertexElement::Tangent]);
    std::vector<glm::vec2> uvs(vertexElementsSizeBytes[(u32)assetLib::VertexElement::UV]);
    std::vector<assetLib::ModelInfo::IndexType> indices(modelInfo.IndicesSizeBytes());
    std::vector<assetLib::ModelInfo::Meshlet> meshlets(modelInfo.MeshletsSizeBytes());

    assetLib::unpackModel(modelInfo, modelFile.Blob.data(), modelFile.Blob.size(),
        {(u8*)positions.data(), (u8*)normals.data(), (u8*)tangents.data(), (u8*)uvs.data()},
        (u8*)indices.data(), (u8*)meshlets.data());

    u32 positionsOffset = 0;
    u32 normalsOffset = 0;
    u32 tangentsOffset = 0;
    u32 uvsOffset = 0;
    u32 indicesOffset = 0;
    u32 meshletsOffset = 0;
    
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
        
        auto tangentsBegin = tangents.begin() + tangentsOffset;
        tangentsOffset += (u32)(meshInfo.VertexElementsSizeBytes[(u32)assetLib::VertexElement::Tangent] / sizeof(glm::vec3));
        auto tangentsEnd = tangents.begin() + tangentsOffset;

        auto uvsBegin = uvs.begin() + uvsOffset;
        uvsOffset += (u32)(meshInfo.VertexElementsSizeBytes[(u32)assetLib::VertexElement::UV] / sizeof(glm::vec2));
        auto uvsEnd = uvs.begin() + uvsOffset;

        auto indicesBegin = indices.begin() + indicesOffset;
        indicesOffset += (u32)(meshInfo.IndicesSizeBytes / sizeof(assetLib::ModelInfo::IndexType));
        auto indicesEnd = indices.begin() + indicesOffset;

        auto meshletsBegin = meshlets.begin() + meshletsOffset;
        meshletsOffset += (u32)(meshInfo.MeshletsSizeBytes / sizeof(assetLib::ModelInfo::Meshlet));
        auto meshletsEnd = meshlets.begin() + meshletsOffset;
        
        assetLib::ModelInfo::MaterialInfo& albedo = meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialAspect::Albedo];
        assetLib::ModelInfo::MaterialInfo& normal = meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialAspect::Normal];
        assetLib::ModelInfo::MaterialInfo& metallicRoughness =
            meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialAspect::MetallicRoughness];
        assetLib::ModelInfo::MaterialInfo& ambientOcclusion =
            meshInfo.Materials[(u32)assetLib::ModelInfo::MaterialAspect::AmbientOcclusion];
        MaterialInfo material = {
            .Type = meshInfo.MaterialType,
            .PropertiesPBR = meshInfo.MaterialPropertiesPBR,
            .AlbedoTextures = albedo.Textures,
            .NormalTextures = normal.Textures,
            .MetallicRoughnessTextures = metallicRoughness.Textures,
            .AmbientOcclusionTextures = ambientOcclusion.Textures};

        meshes.push_back(
            {Mesh(
                std::vector(positionsBegin, positionsEnd),
                std::vector(normalsBegin, normalsEnd),
                std::vector(tangentsBegin, tangentsEnd),
                std::vector(uvsBegin, uvsEnd),
                std::vector(indicesBegin, indicesEnd),
                meshInfo.BoundingSphere,
                std::vector(meshletsBegin, meshletsEnd)),
                material});
    }

    model.m_Meshes = meshes;

    AssetManager::AddModel(std::string{path}, model);
    return AssetManager::GetModel(std::string{path}); 
}

void Model::CreateRenderObjects(Scene* scene, const glm::mat4& transform)
{
    if (!m_IsInScene)
    {
        m_IsInScene = true;
        m_RenderObjects.reserve(m_Meshes.size());
        for (auto& mesh : m_Meshes)
        {
            MaterialGPU material;
            material.Albedo = mesh.Material.PropertiesPBR.Albedo;
            material.Metallic = mesh.Material.PropertiesPBR.Metallic;
            material.Roughness = mesh.Material.PropertiesPBR.Roughness;

            auto addTexture = [](MaterialGPU& materialGPU, const std::vector<std::string>& textures, auto&& fn)
            {
                if (!textures.empty())
                {
                    Image texture = Image::Builder()
                        .FromAssetFile(textures.front())
                        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                        .CreateMipmaps(true)
                        .Build();

                    fn(materialGPU, texture);
                }
            };

            addTexture(material, mesh.Material.AlbedoTextures,
                [&scene](MaterialGPU& materialGPU, const Image& texture)
                {
                    scene->SetMaterialAlbedoTexture(materialGPU, texture);
                });

            addTexture(material, mesh.Material.NormalTextures,
                [&scene](MaterialGPU& materialGPU, const Image& texture)
                {
                    scene->SetMaterialNormalTexture(materialGPU, texture);
                });

            addTexture(material, mesh.Material.MetallicRoughnessTextures,
                [&scene](MaterialGPU& materialGPU, const Image& texture)
                {
                    scene->SetMaterialMetallicRoughnessTexture(materialGPU, texture);
                });
            
            addTexture(material, mesh.Material.AmbientOcclusionTextures,
                [&scene](MaterialGPU& materialGPU, const Image& texture)
                {
                    scene->SetMaterialAmbientOcclusionTexture(materialGPU, texture);
                });
            
            RenderObject renderObject = {};
            renderObject.Mesh = scene->AddMesh(mesh.Mesh);
            renderObject.MaterialGPU = scene->AddMaterialGPU(material);
            m_RenderObjects.push_back(renderObject);
        }
    }

    for (auto& renderObject : m_RenderObjects)
    {
        renderObject.Transform = transform;
        scene->AddRenderObject(renderObject);
    }
}

void Model::CreateDebugBoundingSpheres(Scene* scene, const glm::mat4& transform)
{
    Model* sphere = scene->GetModel("sphere");
    if (sphere == nullptr)
    {
        LOG("Debug 'sphere' model is absent");
        return;
    }

    for (auto& mesh : m_Meshes)
    {
        for (auto& meshlet : mesh.Mesh.GetMeshlets())
        {
            glm::mat4 fullTransform = transform *
                glm::translate(glm::mat4(1.0f), meshlet.BoundingSphere.Center) *
                glm::scale(glm::mat4(1.0), glm::vec3(meshlet.BoundingSphere.Radius));

            sphere->CreateRenderObjects(scene, fullTransform);
        }
    }
}

void ModelCollection::AddModel(Model* model, const std::string& name)
{
    if (m_Models.contains(name))
    {
        if (m_Models.at(name).Model != model)
            LOG("WARNING: overwriting the model {} ({}) with {}",
                name, (void*)m_Models.at(name).Model, (void*)model);
        else
            return;
    }

    m_Models.emplace(name, ModelInfo{
        .Model = model,
        .RenderObjects = CreateRenderObjects(model)});
}

void ModelCollection::ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const
{
    for (u32 textureIndex = 0; textureIndex < m_Textures.size(); textureIndex++)
    {
        const Texture& texture = m_Textures[textureIndex];
        bindlessDescriptorSet.SetTexture("u_textures", texture, textureIndex);
    }
}

std::vector<RenderObject> ModelCollection::CreateRenderObjects(Model* model)
{
    std::vector<RenderObject> renderObjects;
    renderObjects.reserve(m_Meshes.size());
    for (auto& mesh : model->m_Meshes)
    {
        MaterialGPU material;
        material.Albedo = mesh.Material.PropertiesPBR.Albedo;
        material.Metallic = mesh.Material.PropertiesPBR.Metallic;
        material.Roughness = mesh.Material.PropertiesPBR.Roughness;
        auto addTexture = [](MaterialGPU& materialGPU, const std::vector<std::string>& textures, auto&& fn)
        {
            if (!textures.empty())
            {
                Image texture = Image::Builder()
                    .FromAssetFile(textures.front())
                    .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                    .CreateMipmaps(true)
                    .Build();

                fn(materialGPU, texture);
            }
        };

        addTexture(material, mesh.Material.AlbedoTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.AlbedoTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.NormalTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.NormalTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.MetallicRoughnessTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.MetallicRoughnessTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.AmbientOcclusionTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.AmbientOcclusionTextureHandle = AddTexture(texture);
            });
            
        RenderObject renderObject = {};
        renderObject.Mesh = AddMesh(mesh.Mesh);
        renderObject.MaterialGPU = AddMaterialGPU(material);
        renderObjects.push_back(renderObject);
    }

    return renderObjects;
}

RenderHandle<MaterialGPU> ModelCollection::AddMaterialGPU(const MaterialGPU& material)
{
    return AddRenderHandle(material, m_MaterialsGPU);
}

RenderHandle<Mesh> ModelCollection::AddMesh(const Mesh& mesh)
{
    return AddRenderHandle(mesh, m_Meshes);
}

RenderHandle<Texture> ModelCollection::AddTexture(const Texture& texture)
{
    return AddRenderHandle(texture, m_Textures);
}
