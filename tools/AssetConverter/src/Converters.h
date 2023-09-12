#pragma once
#include <filesystem>

class TextureConverter
{
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".tx";
};

class MeshConverter
{
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".msh";
};

class ShaderConverter
{
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".spv";
};