#pragma once
#include <filesystem>

#include "AssetLib.h"
#include "SceneAsset.h"
#include "ShaderAsset.h"
#include "TextureAsset.h"

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

class TextureConverter
{
public:
    static std::vector<std::string> GetWatchedExtensions() { return WATCHED_EXTENSIONS; }
    static bool WatchesExtension(std::string extension)
    {
        return std::ranges::find(WATCHED_EXTENSIONS, extension) != WATCHED_EXTENSIONS.end();
    }
    
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path,
        assetLib::TextureFormat format);
public:
    static inline const std::vector<std::string> WATCHED_EXTENSIONS = {".png", ".jpg", ".jpeg", ".hdr"};
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".tx";
};

class ShaderStageConverter
{
    using DescriptorFlags = assetLib::ShaderStageInfo::DescriptorSet::DescriptorFlags;
    struct DescriptorFlagInfo
    {
        DescriptorFlags Flags;
        std::string DescriptorName;
    };
    struct InputAttributeBindingInfo
    {
        u32 Binding;
        std::string Attribute;
    };
public:
    struct Options
    {
        std::vector<std::pair<std::string, std::string>> Defines;
        u64 DefinesHash{0};
    };
public:
    static std::vector<std::string> GetWatchedExtensions() { return WATCHED_EXTENSIONS; }
    static bool WatchesExtension(std::string_view extension)
    {
        return std::ranges::find(WATCHED_EXTENSIONS, extension) != WATCHED_EXTENSIONS.end();
    }
    
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path,
        const Options& options = Options{});
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static std::optional<assetLib::ShaderStageInfo> Bake(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& path, const Options& options = Options{});
    static std::string GetBakedFileName(const std::filesystem::path& path, const Options& options = Options{});
private:
    static std::vector<DescriptorFlagInfo> ReadDescriptorsFlags(std::string_view shaderSource);
    static std::vector<InputAttributeBindingInfo> ReadInputBindings(std::string_view shaderSource);
    static void RemoveMetaKeywords(std::string& shaderSource);
    static assetLib::ShaderStageInfo Reflect(const std::vector<u32>& spirV,
        const std::vector<DescriptorFlagInfo>& flags, const std::vector<InputAttributeBindingInfo>& inputBindings);
public:
    static inline const std::vector<std::string> WATCHED_EXTENSIONS = {".vert", ".frag", ".comp"};
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".stage";
    static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 4;

    static constexpr std::string_view META_KEYWORD_PREFIX = "@";
};

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