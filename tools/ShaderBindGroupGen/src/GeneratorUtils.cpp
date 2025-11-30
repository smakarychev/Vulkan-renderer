#include "GeneratorUtils.h"

namespace utils
{
namespace 
{
std::string canonicalizeName(const std::string& name, bool firstIsUppercase)
{
    auto toUpper = [](char c) {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    };
    auto toLower = [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    
    std::string canonicalized;
    canonicalized.reserve(name.size());
    bool nextUppercase = firstIsUppercase;
    bool writeAlpha = false;
    for (char c : name)
    {
        switch (c)
        {
        case '_':
        case '-':
            nextUppercase = firstIsUppercase || writeAlpha;
            break;
        default:
            writeAlpha = true;
            if (nextUppercase)
            {
                canonicalized.push_back(toUpper(c));
                nextUppercase = false;
            }
            else
            {
                canonicalized.push_back(canonicalized.empty() ? toLower(c) : c);
            }
        }
    }

    return canonicalized;
}
}

std::string canonicalizeName(const std::string& name)
{
    return canonicalizeName(name, true);
}

std::string canonicalizeParameterName(const std::string& name)
{
    return canonicalizeName(name, false);
}

std::filesystem::path canonicalizePath(const std::filesystem::path& path, const std::string& extension)
{
    namespace fs = std::filesystem;
    fs::path currentPath = path;
    fs::path canonicalizedPath = canonicalizeName(currentPath.stem().string()) + extension;
    for (currentPath = currentPath.parent_path(); currentPath.has_stem(); currentPath = currentPath.parent_path())
        canonicalizedPath = fs::path(canonicalizeName(currentPath.stem().string())) / canonicalizedPath; 

    return canonicalizedPath;
}

std::string_view getPreamble()
{
    return ""
           "/* This file was automatically generated */\n\n"
           "#pragma once\n\n"
           "#include \"types.h\"";    
}

std::string getIncludeString(const std::string& includePathString)
{
    return std::format("#include \"{}\"", includePathString);
}
}
