#pragma once

#include "AssetLib.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <memory>

namespace assetLib
{
    enum class VertexFormat : u32
    {
        Unknown,
        P3N3UV2,
    };

    struct VertexGroup
    {
        VertexFormat GetVertexFormat();
        std::vector<const void*> Elements();
        std::vector<u64> ElementsSizesBytes();

        std::vector<glm::vec3> Positions;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec2> UVs;
    };

    struct VertexP3N3UV2
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 UV;
    };

    struct ModelInfo : AssetInfoBase
    {
        enum class MaterialType : u32
        {
            Albedo = 0, MaxTypeVal
        };
        struct MaterialInfo
        {
            glm::vec4 Color;
            std::vector<std::string> Textures;
        };
        struct MeshInfo
        {
            std::string Name;
            std::vector<u64> VertexElementsSizeBytes;
            u64 IndicesSizeBytes;
            std::array<MaterialInfo, (u32)MaterialType::MaxTypeVal> Materials;
        };
        
        VertexFormat VertexFormat;
        std::vector<MeshInfo> MeshInfos;
        
        std::vector<u64> VertexElementsSizeBytes() const;
        u64 IndicesSizeBytes() const;
    };

    ModelInfo readModelInfo(const assetLib::File& file);

    assetLib::File packModel(const ModelInfo& info, std::vector<const void*> vertices, void* indices);
    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, std::vector<u8*> vertices, u8* indices);
}
