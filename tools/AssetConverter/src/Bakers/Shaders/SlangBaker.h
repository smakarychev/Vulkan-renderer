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
};

class Slang
{
public:
    static constexpr std::string_view TYPE_REFLECTION_TARGET = "target";
    static constexpr std::string_view TYPE_REFLECTION_NAME = "name";
    static constexpr std::string_view TYPE_REFLECTION_TYPE = "type";
    static constexpr std::string_view TYPE_REFLECTION_TYPE_STRUCT_NAME = "type_name";
    static constexpr std::string_view TYPE_REFLECTION_STRUCT_FIELDS = "fields";
    static constexpr std::string_view TYPE_REFLECTION_ARRAY_ELEMENT_COUNT = "element_count";
    static constexpr std::string_view TYPE_REFLECTION_ARRAY_ELEMENT = "element";
    static constexpr std::string_view TYPE_REFLECTION_BASE_PATH = "uniform_types";
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
