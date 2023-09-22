#include "Model.h"

#include "AssetLib.h"
#include "core.h"
#include "Mesh.h"
#include "ModelAsset.h"

Model Model::LoadFromAsset(std::string_view path)
{
    Model model = {};
    
    assetLib::File modelFile;
    assetLib::loadBinaryFile(path, modelFile);
    assetLib::ModelInfo modelInfo = assetLib::readModelInfo(modelFile);
    ASSERT(modelInfo.VertexFormat == assetLib::VertexFormat::P3N3C3UV2, "Unsupported vertex format")

    std::vector<Vertex3D> vertices(modelInfo.VerticesSizeBytes());
    std::vector<u32> indices(modelInfo.IndicesSizeBytes());
    std::vector<std::string> textures(modelInfo.TexturesSizeBytes());

    assetLib::unpackModel(modelInfo, modelFile.Blob.data(), modelFile.Blob.size(), (u8*)vertices.data(), (u8*)indices.data(), (u8*)textures.data());

    u32 verticesOffset = 0;
    u32 indicesOffset = 0;
    u32 texturesOffset = 0;
    
    std::vector<Mesh> meshes;
    meshes.reserve(modelInfo.MeshInfos.size());
    for (auto& meshInfo : modelInfo.MeshInfos)
    {
        auto verticesBegin = vertices.begin() + verticesOffset;
        verticesOffset += (u32)(meshInfo.VerticesSizeBytes / sizeof(Vertex3D));
        auto verticesEnd = vertices.begin() + verticesOffset;

        auto indicesBegin = indices.begin() + indicesOffset;
        indicesOffset += (u32)(meshInfo.IndicesSizeBytes / sizeof(u32));
        auto indicesEnd = indices.begin() + indicesOffset;

        meshes.emplace_back(std::vector(verticesBegin, verticesEnd), std::vector(indicesBegin, indicesEnd));
    }

    model.m_Meshes = meshes;
    
    return model; 
}

void Model::Upload(const Renderer& renderer)
{
    for (auto& mesh : m_Meshes)
        mesh.Upload(renderer);
}
