#pragma once

#include "AssetLib.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>

namespace assetLib
{
    enum class VertexFormat : u32
    {
        Unknown,
        P3N3T3UV2,
    };

    enum class VertexElement : u32
    {
        Position = 0, Normal, Tangent, UV,
        MaxVal
    };

    struct VertexGroup
    {
        std::array<const void*, (u32)VertexElement::MaxVal> Elements();
        std::array<u64, (u32)VertexElement::MaxVal> ElementsSizesBytes();

        std::vector<glm::vec3> Positions;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec4> Tangents;
        std::vector<glm::vec2> UVs;
    };

    struct VertexP3N3UV2
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent;
        glm::vec2 UV;
    };

    struct BoundingSphere
    {
        glm::vec3 Center;
        f32 Radius;
    };
    
    struct BoundingBox
    {
        glm::vec3 Min;
        glm::vec3 Max;
    };

    struct BoundingCone
    {
        i8 AxisX;
        i8 AxisY;
        i8 AxisZ;
        i8 Cutoff;
    };

    static_assert(sizeof(BoundingSphere) == sizeof(glm::vec4), "The size of bounding sphere must be equal to the size of 4 f32s");
    static_assert(sizeof(BoundingCone) == sizeof(u32), "The size of bounding cone must be equal to the size of u32");
    
    struct ModelInfo : AssetInfoBase
    {
        static constexpr u32 TRIANGLES_PER_MESHLET = 256;
        static constexpr u32 VERTICES_PER_MESHLET = 255;
        using IndexType = u8;
        enum class MaterialType : u32
        {
            Opaque = 0, Translucent 
        };
        enum class MaterialAspect : u32
        {
            Albedo = 0, Normal, MetallicRoughness, AmbientOcclusion, Emissive,
            MaxVal
        };
        struct MaterialPropertiesPBR
        {
            glm::vec4 Albedo;
            f32 Metallic;
            f32 Roughness;
        };
        struct MaterialInfo
        {
            std::vector<std::string> Textures;
        };
        struct Meshlet
        {
            u32 FirstIndex{};
            u32 IndexCount{};

            u32 FirstVertex{};
            u32 VertexCount{};

            BoundingSphere BoundingSphere{};
            BoundingCone BoundingCone{};
        };
        struct MeshInfo
        {
            std::string Name;
            std::array<u64, (u32)VertexElement::MaxVal> VertexElementsSizeBytes;
            u64 IndicesSizeBytes;
            u64 MeshletsSizeBytes;
            MaterialType MaterialType;
            MaterialPropertiesPBR MaterialPropertiesPBR;
            std::array<MaterialInfo, (u32)MaterialAspect::MaxVal> Materials;
            BoundingSphere BoundingSphere;
            BoundingBox BoundingBox;
        };
        
        VertexFormat VertexFormat;
        std::vector<MeshInfo> MeshInfos;
        
        std::vector<u64> VertexElementsSizeBytes() const;
        u64 IndicesSizeBytes() const;
        u64 MeshletsSizeBytes() const;
    };

    ModelInfo readModelInfo(const assetLib::File& file);

    assetLib::File packModel(const ModelInfo& info, const std::vector<const void*>& vertices, void* indices, void* meshlets);
    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, const std::vector<u8*>& vertices, u8* indices, u8* meshlets);
}
