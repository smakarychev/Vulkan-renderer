#include "MeshAsset.h"
#include "utils.h"

#include <nlm_json.hpp>
#include <lz4.h>
#include <glm/gtx/hash.hpp>


namespace
{
    assetLib::VertexFormat parseFormatString(std::string_view format)
    {
        if (format == "P3N3C3UV2")
            return assetLib::VertexFormat::P3N3C3UV2;
        return assetLib::VertexFormat::Unknown;
    }

    template <typename T>
    void hashCombine(u64& seed, const T& val)
    {
        std::hash<T> hasher;
        seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

namespace assetLib
{
    MeshInfo readMeshInfo(const assetLib::File& file)
    {
        MeshInfo info = {};

        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        std::string vertexFormatString = metadata["format"];
        info.VertexFormat = parseFormatString(vertexFormatString);


        info.VerticesSizeBytes = metadata["vertices_size_bytes"];
        info.IndicesSizeBytes = metadata["indices_size_bytes"];

        std::string compressionString = metadata["compression"];
        info.CompressionMode = parseCompressionModeString(compressionString);
        info.OriginalFile = metadata["original_file"];

        return info;
    }

    assetLib::File packMesh(const MeshInfo& info, void* vertices, void* indices)
    {
        nlohmann::json metadata;

        metadata["format"] = "P3N3C3UV2";
        metadata["vertices_size_bytes"] = info.VerticesSizeBytes;
        metadata["indices_size_bytes"] = info.IndicesSizeBytes;
        metadata["compression"] = "LZ4";
        metadata["original_file"] = info.OriginalFile;

        assetLib::File assetFile = {};
        assetFile.Type = FileType::Mesh;
        assetFile.Version = 1;
        assetFile.JSON = metadata.dump();

        utils::compressToBlob(assetFile.Blob, {vertices, indices}, {info.VerticesSizeBytes, info.IndicesSizeBytes});

        return assetFile;
    }

    void unpackMesh(MeshInfo& info, const u8* source, u64 sourceSizeBytes, u8* vertices, u8* indices)
    {
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != info.VerticesSizeBytes + info.IndicesSizeBytes)
        {
            std::vector<u8> combined(info.VerticesSizeBytes + info.IndicesSizeBytes);
            LZ4_decompress_safe((const char*)source, (char*)combined.data(), (i32)sourceSizeBytes, (i32)combined.size());
            memcpy(vertices, combined.data(), info.VerticesSizeBytes);
            memcpy(indices, combined.data() + info.VerticesSizeBytes, info.IndicesSizeBytes);
        }
        else
        {
            memcpy(vertices, source, info.VerticesSizeBytes);
            memcpy(indices, source + info.VerticesSizeBytes, info.IndicesSizeBytes);
        }
    }    
}

size_t std::hash<assetLib::VertexP3N3C3UV2>::operator()(const assetLib::VertexP3N3C3UV2& vertex) const noexcept
{
    auto&& [p, n, c, uv] = vertex;
    u64 hash = 0;
    hashCombine(hash, vertex.Position);
    hashCombine(hash, vertex.Normal);
    hashCombine(hash, vertex.Color);
    hashCombine(hash, vertex.UV);
    return hash;
}
