#pragma once

#include "AssetLib.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace assetLib
{
    enum class VertexFormat : u32
    {
        Unknown,
        P3N3C3UV2
    };

    struct VertexP3N3C3UV2
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec3 Color;
        glm::vec2 UV;
    };
    
    struct ModelInfo : AssetInfoBase
    {
        struct MeshInfo
        {
            u64 VerticesSizeBytes;
            u64 IndicesSizeBytes;
            u64 TexturesSizeBytes;
        };
        VertexFormat VertexFormat;
        std::vector<MeshInfo> MeshInfos;

        u64 VerticesSizeBytes() const;
        u64 IndicesSizeBytes() const;
        u64 TexturesSizeBytes() const;
    };

    ModelInfo readModelInfo(const assetLib::File& file);

    assetLib::File packModel(const ModelInfo& info, void* vertices, void* indices, void* textures);
    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, u8* vertices, u8* indices, u8* textures);
}
