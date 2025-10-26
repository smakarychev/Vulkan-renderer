#pragma once

#include <string>
#include <filesystem>

namespace utils
{
std::string canonicalizeName(const std::string& name);
std::string canonicalizeParameterName(const std::string& name);
std::filesystem::path canonicalizePath(const std::filesystem::path& path, const std::string& extension);

std::string_view getPreamble();
std::string getIncludeString(const std::string& includePathString);
}
