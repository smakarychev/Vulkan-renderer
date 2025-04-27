#pragma once

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <tiny_gltf.h>
#undef TINYGLTF_NO_INCLUDE_JSON
#undef TINYGLTF_NO_STB_IMAGE_WRITE

#include "Math/Geometry.h"

#include <filesystem>
#include <glm/glm.hpp>

namespace assetLib
{
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
    
    struct SceneInfo
    {
        static constexpr u32 TRIANGLES_PER_MESHLET = 256;
        static constexpr u32 VERTICES_PER_MESHLET = 255;
        using IndexType = u8;
        struct BoundingCone
        {
            i8 AxisX;
            i8 AxisY;
            i8 AxisZ;
            i8 Cutoff;
        };
        struct Meshlet
        {
            u32 FirstIndex{};
            u32 IndexCount{};

            u32 FirstVertex{};
            u32 VertexCount{};

            Sphere BoundingSphere{};
            BoundingCone BoundingCone{};
        };
        
        enum class BufferViewType : u32
        {
            /* vertex attributes */
            Position = 0,
            Normal = 1,
            Tangent = 2,
            Uv = 3,
            
            /* index */
            Index = 4,

            /* per instance data */
            Meshlet = 5,
            
            MaxVal = 6,
        };
        static_assert((u32)BufferViewType::Meshlet == (u32)BufferViewType::MaxVal - 1,
            "Scene relies on Meshlet being MaxVal - 1");
        
        std::filesystem::path Path;
        tinygltf::Model Scene{};
    };

    std::optional<SceneInfo> readSceneHeader(const std::filesystem::path& path);
    bool readSceneBinary(SceneInfo& sceneInfo);

    glm::mat4 getTransform(tinygltf::Node& node);
}
