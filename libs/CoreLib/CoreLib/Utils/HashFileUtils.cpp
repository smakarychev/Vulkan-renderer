#include "HashFileUtils.h"

#include "FileUtils.h"
#include "HashUtils.h"

namespace Hash
{
std::optional<u32> murmur3b32File(const std::filesystem::path& path, u32 seed)
{
    auto read = lux::readFileToBytes(path);
    if (!read.has_value())
        return std::nullopt;

    return murmur3b32((const u8*)read->data(), read->size(), seed);
}
}
