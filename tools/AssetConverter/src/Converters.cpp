#include <iostream>

#include "Converters.h"
#include "types.h"
#include "AssetLib.h"
#include "MeshAsset.h"
#include "TextureAsset.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/optimizer.hpp>

#include <format>

namespace
{
    template <typename Fn>
    bool needsConversion(const std::filesystem::path& path, Fn&& transform)
    {
        std::filesystem::path convertedPath = path;
        transform(convertedPath);

        if (!std::filesystem::exists(convertedPath))
            return true;

        auto originalTime = std::filesystem::last_write_time(path);
        auto convertedTime = std::filesystem::last_write_time(convertedPath);

        if (convertedTime < originalTime)
            return true;

        return false;
    }
}

bool TextureConverter::NeedsConversion(const std::filesystem::path& path)
{
    return needsConversion(path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(TextureConverter::POST_CONVERT_EXTENSION);
    });
}

void TextureConverter::Convert(const std::filesystem::path& path)
{
    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    assetLib::TextureInfo textureInfo = {};
    textureInfo.Format = assetLib::TextureFormat::SRGBA8;
    textureInfo.Dimensions = {.Width = (u32)width, .Height = (u32)height, .Depth = 1};
    textureInfo.SizeBytes = 4llu * width * height; 
    textureInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    textureInfo.OriginalFile = path.string();
    
    assetLib::File textureFile = assetLib::packTexture(textureInfo, pixels);

    std::filesystem::path outPath = path;
    outPath.replace_extension(POST_CONVERT_EXTENSION);
    
    assetLib::saveBinaryFile(outPath.string(), textureFile);

    stbi_image_free(pixels);

    std::cout << std::format("Texture file {} converted to {}\n", path.string(), outPath.string());
}

bool MeshConverter::NeedsConversion(const std::filesystem::path& path)
{
    return needsConversion(path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(MeshConverter::POST_CONVERT_EXTENSION);
    });
}

void MeshConverter::Convert(const std::filesystem::path& path)
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warnings, errors;

    tinyobj::LoadObj(&attributes, &shapes, &materials, &warnings, &errors, path.string().data());

    std::unordered_map<assetLib::VertexP3N3C3UV2, u32> uniqueVertices;
    std::vector<assetLib::VertexP3N3C3UV2> vertices;
    std::vector<u32> indices;
    
    for (auto& shape : shapes)
    {
        for (auto& index : shape.mesh.indices)
        {
            assetLib::VertexP3N3C3UV2 vertex;
            
            vertex.Position[0] = attributes.vertices[3 * index.vertex_index + 0]; 
            vertex.Position[1] = attributes.vertices[3 * index.vertex_index + 1]; 
            vertex.Position[2] = attributes.vertices[3 * index.vertex_index + 2];

            vertex.Normal[0] = attributes.normals[3 * index.normal_index + 0]; 
            vertex.Normal[1] = attributes.normals[3 * index.normal_index + 1]; 
            vertex.Normal[2] = attributes.normals[3 * index.normal_index + 2];

            vertex.UV[0] = attributes.texcoords[2 * index.texcoord_index + 0];
            vertex.UV[1] = attributes.texcoords[2 * index.texcoord_index + 1];

            vertex.Color = vertex.Normal;

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = (u32)vertices.size();
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    assetLib::MeshInfo meshInfo = {};
    meshInfo.VertexFormat = assetLib::VertexFormat::P3N3C3UV2;
    meshInfo.VerticesSizeBytes = vertices.size() * sizeof(assetLib::VertexP3N3C3UV2);
    meshInfo.IndicesSizeBytes = indices.size() * sizeof(u32);
    meshInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    meshInfo.OriginalFile = path.string();

    assetLib::File meshFile = assetLib::packMesh(meshInfo, vertices.data(), indices.data());

    std::filesystem::path outPath = path;
    outPath.replace_extension(POST_CONVERT_EXTENSION);
    
    assetLib::saveBinaryFile(outPath.string(), meshFile);

    std::cout << std::format("Mesh file {} converted to {}\n", path.string(), outPath.string());
}

bool ShaderConverter::NeedsConversion(const std::filesystem::path& path)
{
    return needsConversion(path, [](std::filesystem::path& converted)
    {
        converted.replace_filename(converted.stem().string() + "-" + converted.extension().string().substr(1));
        converted.replace_extension(ShaderConverter::POST_CONVERT_EXTENSION);
    });
}

void ShaderConverter::Convert(const std::filesystem::path& path)
{
    shaderc_shader_kind shaderKind = shaderc_glsl_infer_from_source;
    if (path.extension().string() == ".vert")
        shaderKind = shaderc_vertex_shader;
    else if (path.extension().string() == ".frag")
        shaderKind = shaderc_fragment_shader;

    std::ifstream file(path.string(), std::ios::in | std::ios::binary);
    std::string shaderSource((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderKind, path.string().c_str(), options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cout << std::format("Shader compilation error:\n {}", module.GetErrorMessage());
        return;
    }

    std::vector<u32> spirv = {module.cbegin(), module.cend()};
    std::vector<u32> spirvOptimized;
    spirvOptimized.reserve(spirv.size());
    spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_3);
    optimizer.RegisterPerformancePasses(true);

    if (optimizer.Run(spirv.data(), spirv.size(), &spirvOptimized))
        spirv = spirvOptimized;

    std::filesystem::path outPath = path;
    outPath.replace_filename(outPath.stem().string() + "-" + outPath.extension().string().substr(1));
    outPath.replace_extension(POST_CONVERT_EXTENSION);
    std::ofstream out(outPath, std::ios::binary | std::ios::out);
    out.write((const char*)spirv.data(), (i64)(spirv.size() * sizeof(u32)));

    std::cout << std::format("Shader file {} converted to {}\n", path.string(), outPath.string());
}
