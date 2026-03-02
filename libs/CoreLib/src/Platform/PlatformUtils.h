#pragma once
#include <filesystem>

namespace platform
{
    extern std::filesystem::path getExecutablePath();
    extern void runSubProcess(const std::filesystem::path& executablePath, const std::vector<std::string>& args);
}
