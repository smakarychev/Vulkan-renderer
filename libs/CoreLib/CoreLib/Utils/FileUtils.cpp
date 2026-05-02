#include "FileUtils.h"

#include <CoreLib/types.h>

#include <fstream>

namespace lux
{
Result<std::string, FileError> readFileToString(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    if (!in.good())
        return std::unexpected(FileError{});
    
    const isize metadataSize = in.tellg();
    in.seekg(0, std::ios::beg);
    
    Result<std::string, FileError> read = std::string(metadataSize, 0);
    in.read(read->data(), metadataSize);
    
    return read;
}

Result<void, FileError> writeStringToFile(const std::filesystem::path& path, std::string_view string)
{
    std::ofstream out(path, std::ios::binary);
    if (!out.good())
        return std::unexpected(FileError{});
    
    out.write(string.data(), (isize)string.size());
    
    return {};
}
}
