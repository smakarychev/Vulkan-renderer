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

class TextureConverter
{
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".tx";
};

class ModelConverter
{
public:
    struct MeshData
    {
        std::string Name;
        std::vector<assetLib::VertexP3N3UV2> Vertices;
        std::vector<u32> Indices;
        std::array<assetLib::ModelInfo::MaterialInfo, (u32)assetLib::ModelInfo::MaterialType::MaxTypeVal> MaterialInfos;
    };
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static MeshData ProcessMesh(const aiScene* scene, const aiMesh* mesh, const std::filesystem::path& modelPath);
    static std::vector<assetLib::VertexP3N3UV2> GetMeshVertices(const aiMesh* mesh);
    static std::vector<u32> GetMeshIndices(const aiMesh* mesh);
    static assetLib::ModelInfo::MaterialInfo GetMaterialInfo(const aiMaterial* material, assetLib::ModelInfo::MaterialType type, const std::filesystem::path& modelPath);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".model";
};

class ShaderConverter
{
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static assetLib::ShaderInfo Reflect(const std::vector<u32>& spirV);
public:
    static constexpr std::string_view POST_CONVERT_EXTENSION = ".shader";
    static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 3;
};