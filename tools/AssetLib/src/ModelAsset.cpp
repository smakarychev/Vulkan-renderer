#include "ModelAsset.h"

#include <iostream>
#include <nlm_json.hpp>

#include "lz4.h"
#include "utils.h"

namespace
{
    assetLib::VertexFormat parseFormatString(std::string_view format)
    {
        if (format == "P3N3C3UV2")
            return assetLib::VertexFormat::P3N3C3UV2;
        return assetLib::VertexFormat::Unknown;
    }

    assetLib::ModelInfo::MaterialType parseMaterialString(std::string_view materialString)
    {
        if (materialString == "albedo")
            return assetLib::ModelInfo::MaterialType::Albedo;
        std::cout << "Unrecognized material string\n";
        std::unreachable();
    }

    std::string materialTypeToString(assetLib::ModelInfo::MaterialType type)
    {
        switch (type)
        {
        case assetLib::ModelInfo::MaterialType::Albedo:
            return "albedo";
        default:
            std::cout << "Unsupported material type\n";
            break;
        }
        std::unreachable();
    }
}

namespace glm {
    void to_json(nlohmann::json& j, const glm::vec4& vec)
    {
        j = { { "r", vec.x }, { "g", vec.y }, { "b", vec.z }, { "a", vec.w } };
    }
    
    void from_json(const nlohmann::json& j, glm::vec4& vec)
    {
        vec.x = j.at("r").get<f32>();
        vec.y = j.at("g").get<f32>();
        vec.z = j.at("b").get<f32>();
        vec.w = j.at("a").get<f32>();
    }
}


namespace assetLib
{
    u64 ModelInfo::VerticesSizeBytes() const
    {
        return std::accumulate(MeshInfos.begin(), MeshInfos.end(),
            0llu, [](u64 size, const auto& mesh){ return size + mesh.VerticesSizeBytes; });
    }

    u64 ModelInfo::IndicesSizeBytes() const
    {
        return std::accumulate(MeshInfos.begin(), MeshInfos.end(),
            0llu, [](u64 size, const auto& mesh){ return size + mesh.IndicesSizeBytes; });
    }

    ModelInfo readModelInfo(const assetLib::File& file)
    {
        ModelInfo info = {};

        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        std::string vertexFormatString = metadata["format"];
        info.VertexFormat = parseFormatString(vertexFormatString);
        
        const nlohmann::json& meshes = metadata["meshes_info"];
        info.MeshInfos.reserve(meshes.size());
        for (auto& mesh : meshes)
        {
            ModelInfo::MeshInfo meshInfo = {};

            meshInfo.Name = mesh["name"];
            
            u64 verticesSizeBytes = mesh["vertices_size_bytes"];
            meshInfo.VerticesSizeBytes =  verticesSizeBytes;
            
            u64 indicesSizeBytes = mesh["indices_size_bytes"];
            meshInfo.IndicesSizeBytes = indicesSizeBytes;

            const nlohmann::json& materials = mesh["materials"];
            for (auto& material : materials)
            {
                std::string materialTypeString = material["type"];
                ModelInfo::MaterialType materialType = parseMaterialString(materialTypeString);
                ModelInfo::MaterialInfo materialInfo = {};

                materialInfo.Color = material["color"];
                const nlohmann::json& textures = material["textures"];
                materialInfo.Textures.reserve(textures.size());
                for (auto& texture : textures)
                    materialInfo.Textures.push_back(texture);

                meshInfo.Materials[(u32)materialType] = materialInfo;
            }
            
            info.MeshInfos.push_back(meshInfo);
        }

        unpackAssetInfo(info, &metadata);

        return info;
    }

    assetLib::File packModel(const ModelInfo& info, void* vertices, void* indices)
    {
        nlohmann::json metadata;

        metadata["format"] = "P3N3C3UV2";

        metadata["meshes_info"] = nlohmann::json::array();
        for (auto& mesh : info.MeshInfos)
        {
            nlohmann::json meshJson;
            meshJson["name"] = mesh.Name;
            meshJson["vertices_size_bytes"] = mesh.VerticesSizeBytes;
            meshJson["indices_size_bytes"] = mesh.IndicesSizeBytes;

            meshJson["materials"] = nlohmann::json::array();
            for (u32 i = 0; i < mesh.Materials.size(); i++)
            {
                auto& material = mesh.Materials[i];
                nlohmann::json materialJson;
                materialJson["type"] = materialTypeToString((ModelInfo::MaterialType)i);
                materialJson["color"] = material.Color;

                materialJson["textures"] = nlohmann::json::array();
                for (auto& texture : material.Textures)
                    materialJson["textures"].push_back(texture);

                meshJson["materials"].push_back(materialJson);
            }
            
            metadata["meshes_info"].push_back(meshJson);
        }
        
        packAssetInfo(info, &metadata);
        
        assetLib::File assetFile = {};

        u64 verticesSizeTotal = info.VerticesSizeBytes();
        u64 indicesSizeTotal = info.IndicesSizeBytes();

        u64 blobSizeBytes = utils::compressToBlob(assetFile.Blob, {vertices, indices}, {verticesSizeTotal, indicesSizeTotal});
        metadata["asset"]["blob_size_bytes"] = blobSizeBytes;
        metadata["asset"]["type"] = assetTypeToString(AssetType::Model);

        assetFile.JSON = metadata.dump(JSON_INDENT);

        return assetFile;        
    }

    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, u8* vertices, u8* indices)
    {
        u64 verticesSizeTotal = info.VerticesSizeBytes();
        u64 indicesSizeTotal = info.IndicesSizeBytes();

        u64 totalsize = verticesSizeTotal + indicesSizeTotal;
        
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != verticesSizeTotal + indicesSizeTotal)
        {
            std::vector<u8> combined(totalsize);
            LZ4_decompress_safe((const char*)source, (char*)combined.data(), (i32)sourceSizeBytes, (i32)totalsize);
            memcpy(vertices, combined.data(), verticesSizeTotal);
            memcpy(indices, combined.data() + verticesSizeTotal, indicesSizeTotal);
        }
        else
        {
            memcpy(vertices, source, verticesSizeTotal);
            memcpy(indices, source + verticesSizeTotal, indicesSizeTotal);
        }
    }
}
