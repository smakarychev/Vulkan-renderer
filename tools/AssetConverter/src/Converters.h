#pragma once
#include <filesystem>

#include "AssetLib.h"
#include "ModelAsset.h"
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

class ModelConverter
{
    using IndexType = assetLib::ModelInfo::IndexType;
public:
    struct MeshData
    {
        std::string Name;
        assetLib::VertexGroup VertexGroup;
        std::vector<IndexType> Indices;
        std::vector<assetLib::ModelInfo::Meshlet> Meshlets;
        assetLib::ModelInfo::MaterialType MaterialType;
        assetLib::ModelInfo::MaterialPropertiesPBR MaterialPropertiesPBR;
        std::array<assetLib::ModelInfo::MaterialInfo, (u32)assetLib::ModelInfo::MaterialAspect::MaxVal> MaterialInfos;
    };
public:
    static std::vector<std::string> GetWatchedExtensions() { return WATCHED_EXTENSIONS; }
    static bool WatchesExtension(std::string_view extension)
    {
        return std::ranges::find(WATCHED_EXTENSIONS, extension) != WATCHED_EXTENSIONS.end();
    }
    
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
private:
    static MeshData ProcessMesh(const aiScene* scene, const aiMesh* mesh, const std::filesystem::path& modelPath);
    static assetLib::VertexGroup GetMeshVertices(const aiMesh* mesh);
    static std::vector<u32> GetMeshIndices(const aiMesh* mesh);
    static assetLib::ModelInfo::MaterialInfo GetMaterialInfo(const aiMaterial* material,
        assetLib::ModelInfo::MaterialAspect type, const std::filesystem::path& modelPath);
    static assetLib::ModelInfo::MaterialType GetMaterialType(const aiMaterial* material);
    static assetLib::ModelInfo::MaterialPropertiesPBR GetMaterialPropertiesPBR(const aiMaterial* material);

    static void ConvertTextures(const std::filesystem::path& initialDirectoryPath, MeshData& meshData);
    
public:
    static inline const std::vector<std::string> WATCHED_EXTENSIONS = {".obj", ".fbx", ".blend", ".gltf"};
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".model";
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
    static std::vector<std::string> GetWatchedExtensions() { return WATCHED_EXTENSIONS; }
    static bool WatchesExtension(std::string_view extension)
    {
        return std::ranges::find(WATCHED_EXTENSIONS, extension) != WATCHED_EXTENSIONS.end();
    }
    
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static std::optional<assetLib::ShaderStageInfo> Bake(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& path);
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