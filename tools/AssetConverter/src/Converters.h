#pragma once
#include <filesystem>

#include "AssetLib.h"
#include "ModelAsset.h"
#include "ShaderAsset.h"

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
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".tx";
};

class ModelConverter
{
public:
    struct MeshData
    {
        std::string Name;
        assetLib::VertexGroup VertexGroup;
        std::vector<u16> Indices;
        std::vector<assetLib::ModelInfo::Meshlet> Meshlets;
        std::array<assetLib::ModelInfo::MaterialInfo, (u32)assetLib::ModelInfo::MaterialType::MaxVal> MaterialInfos;
    };
public:
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
private:
    static MeshData ProcessMesh(const aiScene* scene, const aiMesh* mesh, const std::filesystem::path& modelPath);
    static assetLib::VertexGroup GetMeshVertices(const aiMesh* mesh);
    static std::vector<u32> GetMeshIndices(const aiMesh* mesh);
    static assetLib::ModelInfo::MaterialInfo GetMaterialInfo(const aiMaterial* material, assetLib::ModelInfo::MaterialType type, const std::filesystem::path& modelPath);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".model";
};

class ShaderConverter
{
    using DescriptorFlags = assetLib::ShaderInfo::DescriptorSet::DescriptorFlags;
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
    static bool NeedsConversion(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path);
private:
    static std::vector<DescriptorFlagInfo> ReadDescriptorsFlags(std::string_view shaderSource);
    static std::vector<InputAttributeBindingInfo> ReadInputBindings(std::string_view shaderSource);
    static void RemoveMetaKeywords(std::string& shaderSource);
    static assetLib::ShaderInfo Reflect(const std::vector<u32>& spirV,
        const std::vector<DescriptorFlagInfo>& flags, const std::vector<InputAttributeBindingInfo>& inputBindings);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".shader";
    static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 3;

    static constexpr std::string_view META_KEYWORD_PREFIX = "@";
};