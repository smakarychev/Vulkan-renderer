#pragma once

#include "Bakers/BakerContext.h"
#include "types.h"

#include <expected>

namespace bakers
{

struct SlangBakeSetting
{
    std::vector<std::pair<std::string, std::string>> Defines;
    u64 DefinesHash{0};
};

class Slang
{
public:
    BakeResult Bake(const std::filesystem::path& path, const SlangBakeSetting& settings, const Context& ctx);
};

}
