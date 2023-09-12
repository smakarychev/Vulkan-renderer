#pragma once

#include "AssetLib.h"
#include "types.h"

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
        bool operator==(const VertexP3N3C3UV2& other) const
        {
            return Position == other.Position && Normal == other.Normal && Color == other.Color && UV == other.UV;
        }
    };

    struct MeshInfo : AssetInfoBase
    {
        VertexFormat VertexFormat;
        u64 VerticesSizeBytes;
        u64 IndicesSizeBytes;
    };

    MeshInfo readMeshInfo(const assetLib::File& file);

    assetLib::File packMesh(const MeshInfo& info, void* vertices, void* indices);
    void unpackMesh(MeshInfo& info, const u8* source, u64 sourceSizeBytes, u8* vertices, u8* indices);
}

namespace std
{
    template<> struct hash<assetLib::VertexP3N3C3UV2>
    {
        size_t operator()(const assetLib::VertexP3N3C3UV2& vertex) const noexcept;
    };
}