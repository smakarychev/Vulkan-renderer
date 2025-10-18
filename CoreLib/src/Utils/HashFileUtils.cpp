#include "HashFileUtils.h"
#include "HashUtils.h"

#include <fstream>

namespace Hash
{
std::optional<u32> murmur3b32File(const std::filesystem::path& path, u32 seed)
{
    std::ifstream in(path.string(), std::ios::binary | std::ios::ate);
    if (!in.good())
        return std::nullopt;
        
    const isize fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string content(fileSize, 0);
    in.read(content.data(), fileSize);
    in.close();

    return murmur3b32((const u8*)content.data(), fileSize, seed);
}
}
