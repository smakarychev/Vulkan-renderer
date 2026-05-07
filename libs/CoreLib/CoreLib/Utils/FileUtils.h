#pragma once
#include "CoreLib/Containers/Result.h"

#include <filesystem>
#include <string>

namespace lux
{
struct FileError {};
Result<std::string, FileError> readFileToString(const std::filesystem::path& path);
Result<std::vector<std::byte>, FileError> readFileToBytes(const std::filesystem::path& path);
Result<void, FileError> writeStringToFile(const std::filesystem::path& path, std::string_view string);
}
