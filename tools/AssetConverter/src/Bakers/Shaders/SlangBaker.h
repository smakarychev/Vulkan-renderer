#pragma once

#include "Bakers/BakerContext.h"
#include "types.h"
#include "Containers/Result.h"
#include "v2/Shaders/SlangShaderAsset.h"

namespace bakers
{

struct SlangBakeSettings
{
    std::vector<std::pair<std::string, std::string>> Defines;
    u64 DefinesHash{0};
    std::vector<std::string> IncludePaths;
    std::string UniformReflectionDirectoryName{};
};

class Slang
{
public:
    IoResult<void> BakeToFile(const std::filesystem::path& path,
        const SlangBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::ShaderAsset> Bake(const std::filesystem::path& path,
        const SlangBakeSettings& settings, const Context& ctx);
private:
    // todo: this is a temp name until there are conflicts with the old asset system
    static constexpr std::string_view POST_BAKE_EXTENSION = ".sl_shader";
};
}
