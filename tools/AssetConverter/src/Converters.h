#pragma once
#include <filesystem>

#include "AssetLib.h"
#include "ShaderAsset.h"

namespace assetLib
{
    struct VertexP3N3C3UV2;
}

enum aiTextureType : int;
struct aiMaterial;
struct aiMesh;
struct aiScene;

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
        std::vector<assetLib::VertexP3N3C3UV2> Vertices;
        std::vector<u32> Indices;
        std::vector<std::string> Textures;
    };
public:
    static bool NeedsConversion(const std::filesystem::path& path);
    static void Convert(const std::filesystem::path& path);
private:
    static MeshData ProcessMesh(const aiScene* scene, const aiMesh* mesh);
    static std::vector<assetLib::VertexP3N3C3UV2> GetMeshVertices(const aiMesh* mesh);
    static std::vector<u32> GetMeshIndices(const aiMesh* mesh);
    static std::vector<std::string> GetMeshTextures(const aiMaterial* material, aiTextureType textureType);
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