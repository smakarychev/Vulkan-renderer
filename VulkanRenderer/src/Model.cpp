#include "Model.h"

#include "AssetManager.h"
#include "Core/core.h"
#include "Mesh.h"
#include "TextureAsset.h"
#include "Core/Random.h"

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
        Material material = {
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
