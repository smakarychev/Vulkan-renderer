#include "Converters.h"

#include "AssetLib.h"
#include "TextureAsset.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/optimizer.hpp>

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>        
#include <assimp/postprocess.h>  

#include <format>
#include <spirv_reflect.h>
#include <fstream>
#include <iostream>

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
    std::filesystem::path assetPath, blobPath;
    assetPath = blobPath = path;
    assetPath.replace_extension(POST_CONVERT_EXTENSION);
    blobPath.replace_extension(BLOB_EXTENSION);

    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    
    assetLib::TextureInfo textureInfo = {};
    textureInfo.Format = assetLib::TextureFormat::SRGBA8;
    textureInfo.Dimensions = {.Width = (u32)width, .Height = (u32)height, .Depth = 1};
    textureInfo.SizeBytes = 4llu * width * height; 
    textureInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    textureInfo.OriginalFile = path.string();
    textureInfo.BlobFile = blobPath.string();
    
    assetLib::File textureFile = assetLib::packTexture(textureInfo, pixels);
        
    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), textureFile);

    stbi_image_free(pixels);

    std::cout << std::format("Texture file {} converted to {} (blob at {})\n", path.string(), assetPath.string(), blobPath.string());
}


bool ModelConverter::NeedsConversion(const std::filesystem::path& path)
{
    return needsConversion(path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(ModelConverter::POST_CONVERT_EXTENSION);
    });
}

void ModelConverter::Convert(const std::filesystem::path& path)
{
    std::filesystem::path assetPath, blobPath;
    assetPath = blobPath = path;
    assetPath.replace_extension(POST_CONVERT_EXTENSION);
    blobPath.replace_extension(BLOB_EXTENSION);
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(),
       aiProcess_CalcTangentSpace       | 
       aiProcess_Triangulate            |
       aiProcess_JoinIdenticalVertices  |
       aiProcess_SortByPType);

    if (scene == nullptr)
    {
        std::cout << std::format("Failed to load model: {}\n", path.string());
        return;
    }

    using ModelData = MeshData;
    ModelData modelData = {};

    assetLib::ModelInfo modelInfo = {};
    modelInfo.VertexFormat = assetLib::VertexFormat::P3N3C3UV2;
    modelInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    modelInfo.OriginalFile = path.string();
    modelInfo.BlobFile = blobPath.string();

    std::vector<aiNode*> nodesToProcess;
    nodesToProcess.push_back(scene->mRootNode);
    while (!nodesToProcess.empty())
    {
        aiNode* currentNode = nodesToProcess.back(); nodesToProcess.pop_back();

        for (u32 i = 0; i < currentNode->mNumMeshes; i++)
        {
            MeshData meshData = ProcessMesh(scene, scene->mMeshes[currentNode->mMeshes[i]], path);

            modelInfo.MeshInfos.push_back({
                .Name = meshData.Name,
                .VerticesSizeBytes = meshData.Vertices.size() * sizeof(assetLib::VertexP3N3UV2),
                .IndicesSizeBytes = meshData.Indices.size() * sizeof(u32),
                .Materials = meshData.MaterialInfos});

            modelData.Vertices.append_range(meshData.Vertices);
            modelData.Indices.append_range(meshData.Indices);
        }
            
        for (u32 i = 0; i < currentNode->mNumChildren; i++)
            nodesToProcess.push_back(currentNode->mChildren[i]);
    }

    assetLib::File modelFile = assetLib::packModel(modelInfo, modelData.Vertices.data(), modelData.Indices.data());

    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), modelFile);

    std::cout << std::format("Model file {} converted to {} (blob at {})\n\n", path.string(), assetPath.string(), blobPath.string());
}

ModelConverter::MeshData ModelConverter::ProcessMesh(const aiScene* scene, const aiMesh* mesh, const std::filesystem::path& modelPath)
{
    std::vector<assetLib::VertexP3N3UV2> vertices = GetMeshVertices(mesh);

    std::vector<u32> indices = GetMeshIndices(mesh);

    std::array<assetLib::ModelInfo::MaterialInfo, (u32)assetLib::ModelInfo::MaterialType::MaxTypeVal> materials;
    if (scene->HasMaterials())
        for (u32 i = 0; i < materials.size(); i++)
            materials[i] = GetMaterialInfo(scene->mMaterials[mesh->mMaterialIndex], (assetLib::ModelInfo::MaterialType)i, modelPath);

    return {.Name = mesh->mName.C_Str(), .Vertices = vertices, .Indices = indices, .MaterialInfos = materials};
}

std::vector<assetLib::VertexP3N3UV2> ModelConverter::GetMeshVertices(const aiMesh* mesh)
{
    std::vector<assetLib::VertexP3N3UV2> vertices(mesh->mNumVertices);
    for (u32 i = 0; i < mesh->mNumVertices; i++)
    {
        vertices[i].Position = glm::vec3{mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
        vertices[i].Normal = glm::vec3{mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};

        if (mesh->HasTextureCoords(0))
            vertices[i].UV = glm::vec2{mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        else
            vertices[i].UV = glm::vec2{0.0f, 0.0f};
    }

    return vertices;
}

std::vector<u32> ModelConverter::GetMeshIndices(const aiMesh* mesh)
{
    u32 indexCount = 0;
    for (u32 i = 0; i < mesh->mNumFaces; i++)
        indexCount += mesh->mFaces[i].mNumIndices;

    std::vector<u32> indices;
    indices.reserve(indexCount);
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        const aiFace& face = mesh->mFaces[i];
        for (u32 j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);    
    }

    return indices;
}

assetLib::ModelInfo::MaterialInfo ModelConverter::GetMaterialInfo(const aiMaterial* material, assetLib::ModelInfo::MaterialType type, const std::filesystem::path& modelPath)
{
    aiColor4D color;
    aiReturn colorSuccess;
    aiTextureType textureType;
    switch (type)
    {
    case assetLib::ModelInfo::MaterialType::Albedo:
        colorSuccess = material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        textureType = aiTextureType_DIFFUSE;
        break;
    default:
        std::cout << "Unsupported material type";
        return {};
    }
    
    u32 textureCount = material->GetTextureCount(textureType);
    std::vector<std::string> textures(textureCount);
    for (u32 i = 0; i < textureCount; i++)
    {
        aiString textureName;
        material->GetTexture(textureType, i, &textureName);
        std::filesystem::path texturePath = modelPath.parent_path() / std::filesystem::path(textureName.C_Str());
        texturePath.replace_extension(TextureConverter::POST_CONVERT_EXTENSION);
        textures[i] = texturePath.string();
    }

    return {.Color = {color.r, color.g, color.b, color.a}, .Textures = textures};
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
    std::filesystem::path assetPath, blobPath;
    assetPath = blobPath = path;
    assetPath.replace_filename(path.stem().string() + "-" + path.extension().string().substr(1));
    assetPath.replace_extension(POST_CONVERT_EXTENSION);
    blobPath.replace_filename(path.stem().string() + "-" + path.extension().string().substr(1));
    blobPath.replace_extension(BLOB_EXTENSION);
    
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

    // produce reflection on unoptimized code
    assetLib::ShaderInfo shaderInfo = Reflect(spirv);
    shaderInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    shaderInfo.OriginalFile = assetPath.string();
    shaderInfo.BlobFile = blobPath.string();

    std::vector<u32> spirvOptimized;
    spirvOptimized.reserve(spirv.size());
    spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_3);
    optimizer.RegisterPerformancePasses(true);

    if (optimizer.Run(spirv.data(), spirv.size(), &spirvOptimized))
        spirv = spirvOptimized;

    shaderInfo.SourceSizeBytes = spirv.size() * sizeof(u32);
    
    assetLib::File shaderFile = assetLib::packShader(shaderInfo, spirv.data());

    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), shaderFile);

    std::cout << std::format("Shader file {} converted to {} (blob at {})\n", path.string(), assetPath.string(), blobPath.string());
}

assetLib::ShaderInfo ShaderConverter::Reflect(const std::vector<u32>& spirV)
{
    static constexpr u32 SPV_INVALID_VAL = (u32)-1;

    assetLib::ShaderInfo shaderInfo = {};
    
    SpvReflectShaderModule reflectedModule = {};
    spvReflectCreateShaderModule(spirV.size() * sizeof(u32), spirV.data(), &reflectedModule);

    shaderInfo.ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage;

    // extract input attributes
    if (reflectedModule.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    {
        u32 inputCount;
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, inputs.data());

        shaderInfo.InputAttributes.reserve(inputCount);
        for (auto& input : inputs)
        {
            if (input->location == SPV_INVALID_VAL)
                continue;
            shaderInfo.InputAttributes.push_back({
                .Location = input->location,
                .Name = input->name,
                .Format = (VkFormat)input->format
            });
        }
        std::ranges::sort(shaderInfo.InputAttributes,
            [](const auto& a, const auto& b) { return a.Location < b.Location; });
    }

    // extract push constants
    u32 pushCount;
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, nullptr);
    std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, pushConstants.data());

    shaderInfo.PushConstants.reserve(pushCount);
    for (auto& push : pushConstants)
        shaderInfo.PushConstants.push_back({.SizeBytes = push->size, .Offset = push->offset, .ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage});
    std::ranges::sort(shaderInfo.PushConstants,
        [](const auto& a, const auto& b) { return a.Offset < b.Offset; });

    // extract descriptors
    u32 setCount;
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(setCount);
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, sets.data());

    if (setCount > MAX_PIPELINE_DESCRIPTOR_SETS)
    {
        std::cout << std::format("Can have only {} different descriptor sets, but have {}",
            MAX_PIPELINE_DESCRIPTOR_SETS, setCount);
        return shaderInfo;
    }

    shaderInfo.DescriptorSets.reserve(setCount);
    for (auto& set : sets)
    {
        shaderInfo.DescriptorSets.push_back({.Set = set->set});
        
        assetLib::ShaderInfo::DescriptorSet& descriptorSet = shaderInfo.DescriptorSets.back();
        descriptorSet.Bindings.resize(set->binding_count);
        for (u32 i = 0; i < set->binding_count; i++)
        {
            descriptorSet.Bindings[i] = {
                .Binding = set->bindings[i]->binding,
                .Name = set->bindings[i]->name,
                .Descriptor = (VkDescriptorType)set->bindings[i]->descriptor_type,
                .ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage
            };
            if (descriptorSet.Bindings[i].Name.starts_with("dyn"))
            {
                if (descriptorSet.Bindings[i].Descriptor == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    descriptorSet.Bindings[i].Descriptor = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                else if (descriptorSet.Bindings[i].Descriptor == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    descriptorSet.Bindings[i].Descriptor = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }
        }
        std::ranges::sort(descriptorSet.Bindings,
                          [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
    }
    std::ranges::sort(shaderInfo.DescriptorSets,
                      [](const auto& a, const auto& b) { return a.Set < b.Set; });

    return shaderInfo;
}
