#pragma once

#include "v2/AssetLibV2.h"

namespace assetlib
{
struct ShaderLoadInfo
{
    std::string Name{};
    struct EntryPoint
    {
        std::string Name{};
        std::string Path{};
    };
    std::vector<EntryPoint> EntryPoints{};
};

namespace shader
{
io::IoResult<ShaderLoadInfo> readLoadInfo(const std::filesystem::path& path);
}
}

