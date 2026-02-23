#pragma once
#include <filesystem>

#include "SceneAsset.h"

namespace assetLib
{
    struct VertexP3N3UV2;
}

enum aiTextureType : int;
struct aiMaterial;
struct aiMesh;
struct aiScene;

static constexpr std::string_view BLOB_EXTENSION = ".blob";
static constexpr std::string_view RAW_ASSETS_DIRECTORY_NAME = "raw";
static constexpr std::string_view PROCESSED_ASSETS_DIRECTORY_NAME = "processed";

class SceneConverter
{
public:
    static std::vector<std::string> GetWatchedExtensions() { return WATCHED_EXTENSIONS; }
    static bool WatchesExtension(std::string extension)
    {
        return std::ranges::find(WATCHED_EXTENSIONS, extension) != WATCHED_EXTENSIONS.end();
    }
    
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
public:
    static inline const std::vector<std::string> WATCHED_EXTENSIONS = {".gltf"};
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".scene";
};