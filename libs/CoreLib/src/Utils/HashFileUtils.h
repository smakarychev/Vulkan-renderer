#pragma once
#include "types.h"

#include <filesystem>

namespace Hash
{
std::optional<u32> murmur3b32File(const std::filesystem::path& path, u32 seed = 104729);
}
