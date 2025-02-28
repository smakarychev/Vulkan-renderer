#pragma once

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <tiny_gltf.h>
#undef TINYGLTF_NO_INCLUDE_JSON
#undef TINYGLTF_NO_STB_IMAGE_WRITE

#include "types.h"

#include <filesystem>
#include <glm/glm.hpp>

namespace assetLib
{
    struct SceneInfo
    {
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
