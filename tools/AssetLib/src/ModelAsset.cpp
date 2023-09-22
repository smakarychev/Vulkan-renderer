#include "ModelAsset.h"

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

    u64 ModelInfo::TexturesSizeBytes() const
    {
        return std::accumulate(MeshInfos.begin(), MeshInfos.end(),
            0llu, [](u64 size, const auto& mesh){ return size + mesh.TexturesSizeBytes; });
    }

    ModelInfo readModelInfo(const assetLib::File& file)
    {
        ModelInfo info = {};

        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        std::string vertexFormatString = metadata["format"];
        info.VertexFormat = parseFormatString(vertexFormatString);
        
        const nlohmann::json& meshes = metadata["meshes_info"];
        for (auto& mesh : meshes)
        {
            u64 verticesSizeBytes = mesh["vertices_size_bytes"];
            u64 indicesSizeBytes = mesh["indices_size_bytes"];
            u64 texturesSizeBytes = mesh["textures_size_bytes"];

            info.MeshInfos.push_back({
                .VerticesSizeBytes = verticesSizeBytes,
                .IndicesSizeBytes = indicesSizeBytes,
                .TexturesSizeBytes = texturesSizeBytes});
        }

        std::string compressionString = metadata["compression"];
        info.CompressionMode = parseCompressionModeString(compressionString);
        info.OriginalFile = metadata["original_file"];

        return info;
    }

    assetLib::File packModel(const ModelInfo& info, void* vertices, void* indices, void* textures)
    {
        nlohmann::json metadata;

        metadata["format"] = "P3N3C3UV2";

        metadata["meshes_info"] = nlohmann::json::array();
        for (auto& mesh : info.MeshInfos)
        {
            nlohmann::json meshJson;
            meshJson["vertices_size_bytes"] = mesh.VerticesSizeBytes;
            meshJson["indices_size_bytes"] = mesh.IndicesSizeBytes;
            meshJson["textures_size_bytes"] = mesh.TexturesSizeBytes;

            metadata["meshes_info"].push_back(meshJson);
        }
        
        metadata["compression"] = "LZ4";
        metadata["original_file"] = info.OriginalFile;

        assetLib::File assetFile = {};
        assetFile.Type = FileType::Model;
        assetFile.Version = 1;
        assetFile.JSON = metadata.dump();

        u64 verticesSizeTotal = info.VerticesSizeBytes();
        u64 indicesSizeTotal = info.IndicesSizeBytes();
        u64 texturesSizeTotal = info.TexturesSizeBytes();

        utils::compressToBlob(assetFile.Blob, {vertices, indices, textures}, {verticesSizeTotal, indicesSizeTotal, texturesSizeTotal});

        return assetFile;        
    }

    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, u8* vertices, u8* indices, u8* textures)
    {
        u64 verticesSizeTotal = info.VerticesSizeBytes();
        u64 indicesSizeTotal = info.IndicesSizeBytes();
        u64 texturesSizeTotal = info.TexturesSizeBytes();

        u64 totalsize = verticesSizeTotal + indicesSizeTotal + texturesSizeTotal;
        
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != verticesSizeTotal + indicesSizeTotal + texturesSizeTotal)
        {
            std::vector<u8> combined(totalsize);
            LZ4_decompress_safe((const char*)source, (char*)combined.data(), (i32)sourceSizeBytes, (i32)totalsize);
            memcpy(vertices, combined.data(), verticesSizeTotal);
            memcpy(indices, combined.data() + verticesSizeTotal, indicesSizeTotal);
            memcpy(textures, combined.data() + verticesSizeTotal + indicesSizeTotal, texturesSizeTotal);
        }
        else
        {
            memcpy(vertices, source, verticesSizeTotal);
            memcpy(indices, source + verticesSizeTotal, indicesSizeTotal);
            memcpy(textures, source + verticesSizeTotal + indicesSizeTotal, texturesSizeTotal);
        }
    }
}
