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

    enum class VertexElement : u32
    {
        Position = 0, Normal, UV,
        MaxVal
    };

    struct VertexGroup
    {
        std::array<const void*, (u32)VertexElement::MaxVal> Elements();
        std::array<u64, (u32)VertexElement::MaxVal> ElementsSizesBytes();

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
            Albedo = 0, MaxVal
        };
        struct MaterialInfo
        {
            glm::vec4 Color;
            std::vector<std::string> Textures;
        };
        struct MeshInfo
        {
            std::string Name;
            std::array<u64, (u32)VertexElement::MaxVal> VertexElementsSizeBytes;
            u64 IndicesSizeBytes;
            std::array<MaterialInfo, (u32)MaterialType::MaxVal> Materials;
        };
        
        VertexFormat VertexFormat;
        std::vector<MeshInfo> MeshInfos;
        
        std::vector<u64> VertexElementsSizeBytes() const;
        u64 IndicesSizeBytes() const;
    };

    ModelInfo readModelInfo(const assetLib::File& file);

    assetLib::File packModel(const ModelInfo& info, const std::vector<const void*>& vertices, void* indices);
    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, const std::vector<u8*>& vertices, u8* indices);
}
