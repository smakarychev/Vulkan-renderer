#include "Converters.h"

#include "AssetLib.h"
#include "TextureAsset.h"

#define STB_IMAGE_IMPLEMENTATION
#include <execution>
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
#include <ranges>
#include <vulkan/vulkan_core.h>

#include "utils.h"
#include "Core/core.h"

namespace
{
    struct AssetPaths
    {
        std::filesystem::path AssetPath;
        std::filesystem::path BlobPath;
    };
    template <typename Fn>
    AssetPaths getAssetsPath(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& rawFilePath, Fn&& transform)
    {
        std::filesystem::path processedPath = rawFilePath.filename();

        std::filesystem::path currentPath = rawFilePath.parent_path();
        while (currentPath != initialDirectoryPath)
        {
            if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
                break;
            processedPath = currentPath.filename() / processedPath;
            currentPath = currentPath.parent_path();
        }

        if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
            processedPath = currentPath.parent_path() / PROCESSED_ASSETS_DIRECTORY_NAME / processedPath;
        else
            processedPath = rawFilePath;

        AssetPaths paths = transform(processedPath);

        return paths;
    }
    
    template <typename Fn>
    bool needsConversion(const std::filesystem::path& initialDirectoryPath,
        const std::filesystem::path& path, Fn&& transform)
    {
        std::filesystem::path convertedPath = getAssetsPath(initialDirectoryPath, path,
            [&transform](std::filesystem::path& unprocessedPath)
            {
                AssetPaths paths;
                paths.AssetPath = unprocessedPath;
                transform(paths.AssetPath);
                return paths;
            }).AssetPath;

        if (!std::filesystem::exists(convertedPath))
            return true;

        auto originalTime = std::filesystem::last_write_time(path);
        auto convertedTime = std::filesystem::last_write_time(convertedPath);

        if (convertedTime < originalTime)
            return true;

        return false;
    }
}

bool TextureConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    return needsConversion(initialDirectoryPath, path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(TextureConverter::POST_CONVERT_EXTENSION);
    });
}

void TextureConverter::Convert(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    Convert(initialDirectoryPath, path, assetLib::TextureFormat::SRGBA8);
}

void TextureConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path,
    assetLib::TextureFormat format)
{
    std::cout << std::format("Converting texture file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });

    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8* pixels{nullptr};
    u64 sizeBytes{0};
    if (path.extension() == ".hdr")
    {
        pixels = (u8*)stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);    
        format = assetLib::TextureFormat::RGBA32;
        sizeBytes = 16llu * width * height; 
    }
    else
    {
        pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        sizeBytes = 4llu * width * height; 
    }

    assetLib::TextureInfo textureInfo = {};
    textureInfo.Format = format;
    textureInfo.Dimensions = {.Width = (u32)width, .Height = (u32)height, .Depth = 1};
    textureInfo.SizeBytes = sizeBytes; 
    textureInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    textureInfo.OriginalFile = path.string();
    textureInfo.BlobFile = blobPath.string();
    
    assetLib::File textureFile = assetLib::packTexture(textureInfo, pixels);
        
    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), textureFile);

    stbi_image_free(pixels);

    std::cout << std::format("Texture file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}

bool ModelConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    return needsConversion(initialDirectoryPath, path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(ModelConverter::POST_CONVERT_EXTENSION);
    });
}

void ModelConverter::Convert(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    std::cout << std::format("Converting model file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(),
       aiProcess_GenUVCoords            |
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
    modelInfo.VertexFormat = assetLib::VertexFormat::P3N3T3UV2;
    modelInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    modelInfo.OriginalFile = path.string();
    modelInfo.BlobFile = blobPath.string();

    std::vector<aiNode*> nodesToProcess;
    nodesToProcess.push_back(scene->mRootNode);
    while (!nodesToProcess.empty())
    {
        aiNode* currentNode = nodesToProcess.back(); nodesToProcess.pop_back();

        auto meshes = std::ranges::views::iota(0u, currentNode->mNumMeshes);
        std::mutex mutex{};
        std::for_each(std::execution::par_unseq, meshes.begin(), meshes.end(), [&](u32 meshIndex)
        {
            std::cout << std::format("\tConverting mesh {} out of {}\n", modelInfo.MeshInfos.size(), scene->mNumMeshes);
            
            MeshData meshData = ProcessMesh(scene, scene->mMeshes[currentNode->mMeshes[meshIndex]], path);
            ConvertTextures(initialDirectoryPath, meshData);
            assetLib::BoundingSphere sphere = utils::welzlSphere(meshData.VertexGroup.Positions);

            std::lock_guard lock(mutex);
            modelInfo.MeshInfos.push_back({
                .Name = meshData.Name,
                .VertexElementsSizeBytes = meshData.VertexGroup.ElementsSizesBytes(),
                .IndicesSizeBytes = meshData.Indices.size() * sizeof(IndexType),
                .MeshletsSizeBytes = meshData.Meshlets.size() * sizeof(assetLib::ModelInfo::Meshlet),
                .MaterialType = meshData.MaterialType,
                .MaterialPropertiesPBR = meshData.MaterialPropertiesPBR,
                .Materials = meshData.MaterialInfos,
                .BoundingSphere = sphere});

            modelData.VertexGroup.Positions.append_range(meshData.VertexGroup.Positions);
            modelData.VertexGroup.Normals.append_range(meshData.VertexGroup.Normals);
            modelData.VertexGroup.Tangents.append_range(meshData.VertexGroup.Tangents);
            modelData.VertexGroup.UVs.append_range(meshData.VertexGroup.UVs);
            modelData.Indices.append_range(meshData.Indices);
            modelData.Meshlets.append_range(meshData.Meshlets);
        });
            
        for (u32 i = 0; i < currentNode->mNumChildren; i++)
            nodesToProcess.push_back(currentNode->mChildren[i]);
    }
    
    std::array<const void*, (u32)assetLib::VertexElement::MaxVal> vertexElements = modelData.VertexGroup.Elements();
    assetLib::File modelFile = assetLib::packModel(modelInfo, {vertexElements.begin(), vertexElements.end()},
        modelData.Indices.data(), modelData.Meshlets.data());

    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), modelFile);

    std::cout << std::format("Model file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}

ModelConverter::MeshData ModelConverter::ProcessMesh(const aiScene* scene, const aiMesh* mesh,
    const std::filesystem::path& modelPath)
{
    assetLib::VertexGroup vertexGroup = GetMeshVertices(mesh);

    std::vector<u32> indices = GetMeshIndices(mesh);

    assetLib::ModelInfo::MaterialType materialType = assetLib::ModelInfo::MaterialType::Opaque;
    assetLib::ModelInfo::MaterialPropertiesPBR materialPropertiesPBR = {};
    std::array<assetLib::ModelInfo::MaterialInfo, (u32)assetLib::ModelInfo::MaterialAspect::MaxVal> materials;
    if (scene->HasMaterials())
    {
        materialType = GetMaterialType(scene->mMaterials[mesh->mMaterialIndex]);
        materialPropertiesPBR = GetMaterialPropertiesPBR(scene->mMaterials[mesh->mMaterialIndex]);
        for (u32 i = 0; i < materials.size(); i++)
            materials[i] = GetMaterialInfo(scene->mMaterials[mesh->mMaterialIndex],
                (assetLib::ModelInfo::MaterialAspect)i, modelPath);
    }

    MeshData meshData = {
        .Name = mesh->mName.C_Str(),
        .VertexGroup = vertexGroup,
        .MaterialType = materialType,
        .MaterialPropertiesPBR = materialPropertiesPBR,
        .MaterialInfos = materials};
    utils::remapMesh(meshData, indices);
    meshData.Meshlets = utils::createMeshlets(meshData, indices);
    
    return meshData;
}

assetLib::VertexGroup ModelConverter::GetMeshVertices(const aiMesh* mesh)
{
    assetLib::VertexGroup vertexGroup;
    vertexGroup.Positions.resize(mesh->mNumVertices);
    vertexGroup.Normals.resize(mesh->mNumVertices);
    vertexGroup.Tangents.resize(mesh->mNumVertices);
    vertexGroup.UVs.resize(mesh->mNumVertices);
    for (u32 i = 0; i < mesh->mNumVertices; i++)
    {
        vertexGroup.Positions[i] = glm::vec3{mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};

        if (mesh->HasNormals())
            vertexGroup.Normals[i] = glm::vec3{mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
        else
            vertexGroup.Normals[i] = glm::vec3{0.0f, 0.0f, 0.0f};

        if (mesh->HasTangentsAndBitangents())
            vertexGroup.Tangents[i] = glm::vec3{mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
        else
            vertexGroup.Tangents[i] = glm::vec3{0.0f, 0.0f, 0.0f};
        
        if (mesh->HasTextureCoords(0))
            vertexGroup.UVs[i] = glm::vec2{mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        else
            vertexGroup.UVs[i] = glm::vec2{0.0f, 0.0f};
    }

    return vertexGroup;
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

assetLib::ModelInfo::MaterialInfo ModelConverter::GetMaterialInfo(const aiMaterial* material,
    assetLib::ModelInfo::MaterialAspect type, const std::filesystem::path& modelPath)
{
    aiTextureType textureType;
    switch (type)
    {
    case assetLib::ModelInfo::MaterialAspect::Albedo:
        textureType = aiTextureType_BASE_COLOR;
        break;
    case assetLib::ModelInfo::MaterialAspect::Normal:
        textureType = aiTextureType_NORMALS;
        break;
    case assetLib::ModelInfo::MaterialAspect::MetallicRoughness:
        textureType = aiTextureType_METALNESS;
        break;
    case assetLib::ModelInfo::MaterialAspect::AmbientOcclusion:
        textureType = aiTextureType_AMBIENT_OCCLUSION;
        break;
    case assetLib::ModelInfo::MaterialAspect::Emissive:
        textureType = aiTextureType_EMISSION_COLOR;
        break;
    default:
        std::cout << "Unsupported material type";
        return {};
    }
    
    u32 textureCount = material->GetTextureCount(textureType);
    if (textureCount == 0 && textureType == aiTextureType_BASE_COLOR)
    {
        textureType = aiTextureType_DIFFUSE;
        textureCount = material->GetTextureCount(aiTextureType_DIFFUSE);
    }
        
    std::vector<std::string> textures(textureCount);
    for (u32 i = 0; i < textureCount; i++)
    {
        aiString textureName;
        material->GetTexture(textureType, i, &textureName);
        std::filesystem::path texturePath = modelPath.parent_path() / std::filesystem::path(textureName.C_Str());
        textures[i] = texturePath.string();
    }

    return {.Textures = textures};
}

assetLib::ModelInfo::MaterialType ModelConverter::GetMaterialType(const aiMaterial* material)
{
    aiColor4D opacityColor = {};
    material->Get(AI_MATKEY_COLOR_DIFFUSE, opacityColor);

    return opacityColor.a < 1.0f ?
        assetLib::ModelInfo::MaterialType::Translucent : assetLib::ModelInfo::MaterialType::Opaque;
}

assetLib::ModelInfo::MaterialPropertiesPBR ModelConverter::GetMaterialPropertiesPBR(const aiMaterial* material)
{
    aiColor4D albedo = {};
    ai_real metallic = {};
    ai_real roughness = {};
    material->Get(AI_MATKEY_BASE_COLOR, albedo);
    material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
    material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);

    return {
        .Albedo = {albedo.r, albedo.g, albedo.b, albedo.a},
        .Metallic = (f32)metallic,
        .Roughness = (f32)roughness};
}

void ModelConverter::ConvertTextures(const std::filesystem::path& initialDirectoryPath, MeshData& meshData)
{
    for (u32 aspect = 0; aspect < (u32)assetLib::ModelInfo::MaterialAspect::MaxVal; aspect++)
    {
        auto aspectType = (assetLib::ModelInfo::MaterialAspect)aspect;
        auto& material = meshData.MaterialInfos[aspect];
        for (auto& texture : material.Textures)
        {
            if (TextureConverter::NeedsConversion(initialDirectoryPath, texture))
            {
                switch (aspectType)
                {
                case assetLib::ModelInfo::MaterialAspect::Albedo:
                case assetLib::ModelInfo::MaterialAspect::Emissive:
                    TextureConverter::Convert(initialDirectoryPath, texture, assetLib::TextureFormat::SRGBA8);    
                    break;
                case assetLib::ModelInfo::MaterialAspect::Normal:
                case assetLib::ModelInfo::MaterialAspect::MetallicRoughness:
                case assetLib::ModelInfo::MaterialAspect::AmbientOcclusion:
                    TextureConverter::Convert(initialDirectoryPath, texture, assetLib::TextureFormat::RGBA8);
                    break;
                default:
                    LOG("Error unrecognized material type for {}", texture);
                    break;
                }
            }
            
            std::filesystem::path texturePath = texture;
            texturePath.replace_extension(TextureConverter::POST_CONVERT_EXTENSION);
            texture = texturePath.string();
        }
    }
}

bool ShaderConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    std::filesystem::path convertedPath = {};
    
    bool requiresConversion = needsConversion(initialDirectoryPath, path, [&](std::filesystem::path& converted)
    {
        converted.replace_filename(converted.stem().string() + "-" + converted.extension().string().substr(1));
        converted.replace_extension(ShaderConverter::POST_CONVERT_EXTENSION);
        convertedPath = converted;
    });

    if (requiresConversion)
        return true;

    assetLib::File shaderFile;
    assetLib::loadAssetFile(convertedPath.string(), shaderFile);
    assetLib::ShaderInfo shaderInfo = assetLib::readShaderInfo(shaderFile);

    for (auto& includedFile : shaderInfo.IncludedFiles)
    {
        auto originalTime = std::filesystem::last_write_time(includedFile);
        auto convertedTime = std::filesystem::last_write_time(convertedPath);

        if (convertedTime < originalTime)
            return true;
    }

    return false;
}

void ShaderConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path)
{
    std::cout << std::format("Converting shader file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_filename(processedPath.stem().string() + "-" +
                processedPath.extension().string().substr(1));
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath.replace_filename(processedPath.stem().string() + "-" +
                processedPath.extension().string().substr(1));
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });
    
    shaderc_shader_kind shaderKind = shaderc_glsl_infer_from_source;
    if (path.extension().string() == ".vert")
        shaderKind = shaderc_vertex_shader;
    else if (path.extension().string() == ".frag")
        shaderKind = shaderc_fragment_shader;
    else if (path.extension().string() == ".comp")
        shaderKind = shaderc_compute_shader;

    std::ifstream file(path.string(), std::ios::in | std::ios::binary);
    std::string shaderSource((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<DescriptorFlagInfo> descriptorFlags = ReadDescriptorsFlags(shaderSource);
    std::vector<InputAttributeBindingInfo> inputBindingsInfo = ReadInputBindings(shaderSource);
    RemoveMetaKeywords(shaderSource);

    class FileIncluder : public shaderc::CompileOptions::IncluderInterface
    {
        struct FileInfo
        {
            std::string FileName;
            std::string FileContent;
        };
    public:
        FileIncluder (std::vector<std::string>* includedFiles)
            : IncluderInterface(), IncludedFiles(includedFiles) {}
        
        shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type,
            const char* requestingSource, size_t includeDepth) override
        {
            if (type != shaderc_include_type_relative)
                return new shaderc_include_result{"", 0,
                    "only relative #include (with '...') is supported",
                    std::string{"only relative #include (with '...') is supported"}.length(), nullptr};

            std::filesystem::path requestedPath = std::filesystem::path{requestingSource}.parent_path() /
                std::filesystem::path{requestedSource};

            std::string fileName = requestedPath.string();
            std::ifstream fileIn(fileName.data(), std::ios::ate | std::ios::binary);
            if (!fileIn.is_open())
                return new shaderc_include_result{"", 0,
                    "#include file not found",
                    std::string{"#include file not found"}.length(), nullptr};
            isize fileSizeBytes = fileIn.tellg();
            fileIn.seekg(0);
            std::string fileContent;
            fileContent.resize(fileSizeBytes);
            fileIn.read(fileContent.data(), fileSizeBytes);

            IncludedFiles->push_back(fileName);
            
            FileInfo* fileInfo = new FileInfo{
                .FileName = fileName,
                .FileContent = fileContent};

            return new shaderc_include_result{
                fileInfo->FileName.data(), fileInfo->FileName.length(),
                fileInfo->FileContent.data(), fileInfo->FileContent.length(),
                fileInfo};
        }
        void ReleaseInclude(shaderc_include_result* data) override
        {
            FileInfo* fileInfo = (FileInfo*)(data->user_data);
            delete fileInfo;
            delete data;
        }

        std::vector<std::string>* IncludedFiles;
    };
    
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    std::vector<std::string> includedFiles;
    options.SetIncluder(std::make_unique<FileIncluder>(&includedFiles));
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderKind,
        path.string().c_str(), options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cout << std::format("Shader compilation error:\n {}", module.GetErrorMessage());
        return;
    }
    std::vector<u32> spirv = {module.cbegin(), module.cend()};

    // produce reflection on unoptimized code
    assetLib::ShaderInfo shaderInfo = Reflect(spirv, descriptorFlags, inputBindingsInfo);
    shaderInfo.IncludedFiles = includedFiles;
    shaderInfo.CompressionMode = assetLib::CompressionMode::LZ4;
    shaderInfo.OriginalFile = path.string();
    shaderInfo.BlobFile = blobPath.string();

    std::vector<u32> spirvOptimized;
    spirvOptimized.reserve(spirv.size());
    spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_3);
    optimizer.RegisterPerformancePasses(true);

    if (optimizer.Run(spirv.data(), spirv.size(), &spirvOptimized))
        spirv = spirvOptimized;

    shaderInfo.SourceSizeBytes = spirv.size() * sizeof(u32);
    
    assetLib::File shaderFile = assetLib::packShader(shaderInfo, spirv.data());

    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), shaderFile);

    std::cout << std::format("Shader file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}

std::vector<ShaderConverter::DescriptorFlagInfo> ShaderConverter::ReadDescriptorsFlags(std::string_view shaderSource)
{
    // weird flex but ok
    std::vector<ShaderConverter::DescriptorFlags> flags = {
        DescriptorFlags::Dynamic,
        DescriptorFlags::Bindless,
        DescriptorFlags::ImmutableSampler,
        DescriptorFlags::ImmutableSamplerNearest,
        DescriptorFlags::ImmutableSamplerClampEdge,
        DescriptorFlags::ImmutableSamplerNearestClampEdge,
        DescriptorFlags::ImmutableSamplerClampBlack,
        DescriptorFlags::ImmutableSamplerNearestClampBlack,
        DescriptorFlags::ImmutableSamplerClampWhite,
        DescriptorFlags::ImmutableSamplerNearestClampWhite,
    };

    std::vector<DescriptorFlagInfo> descriptorFlagsUnmerged = {};
    
    for (auto flag : flags)
    {
        std::string keyword = std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(flag);
            
        usize offset = 0;
        usize pos = shaderSource.find(keyword, offset);
        while (pos != std::string::npos)
        {
            // find descriptor's name
            offset = pos + keyword.length();
            usize curlyPos, semicolonPos;
            curlyPos = shaderSource.find("{", offset);
            semicolonPos = shaderSource.find(";", offset);
            if (semicolonPos == std::string::npos)
                return {}; // error will be reported by shader compiler
            usize endingSemicolon = semicolonPos;
            if (curlyPos != std::string::npos && curlyPos < semicolonPos)
            {
                curlyPos = shaderSource.find("}", curlyPos);
                endingSemicolon = shaderSource.find(";", curlyPos);
            }

            usize textEnd = shaderSource.find_last_not_of(" \t\r\n", endingSemicolon);
            usize textStart = shaderSource.find_last_of(" \t\r\n", textEnd) + 1;

            std::string name = std::string{shaderSource.substr(textStart, textEnd - textStart)};
            if (name.ends_with("[]"))
                name = name.substr(0, name.size() - std::strlen("[]"));
            descriptorFlagsUnmerged.push_back({.Flags = flag, .DescriptorName = name});

            offset = textEnd;
            pos = shaderSource.find(keyword, offset);
        }
    }

    if (descriptorFlagsUnmerged.size() <= 1)
        return descriptorFlagsUnmerged;
    
    std::ranges::sort(descriptorFlagsUnmerged,
        [](const std::string& a, const std::string& b)
        {
            return a.compare(b);
        },
        [](const auto& a)
        {
            return a.DescriptorName;
        });
    
    std::vector<DescriptorFlagInfo> descriptorFlags;
    descriptorFlags.emplace_back(descriptorFlagsUnmerged.front());
    for (u32 i = 1; i < descriptorFlagsUnmerged.size(); i++)
    {
        if (descriptorFlags.back().DescriptorName == descriptorFlagsUnmerged[i].DescriptorName)
            descriptorFlags.back().Flags |= descriptorFlagsUnmerged[i].Flags;
        else
            descriptorFlags.emplace_back(descriptorFlagsUnmerged[i]);
    }
    
    return descriptorFlags;
}

std::vector<ShaderConverter::InputAttributeBindingInfo> ShaderConverter::ReadInputBindings(
    std::string_view shaderSource)
{
    struct AttributeInfo
    {
        usize TextPosition;
        std::string Name;
    };

    struct BindingInfo
    {
        usize TextPosition;
        u32 Binding;
    };
    
    auto findAttributes = [](std::string_view source) -> std::vector<AttributeInfo>
    {
        std::vector<AttributeInfo> attributes;

        static std::string keyword = "layout";
                
        usize offset = 0;
        usize startPos = source.find(keyword, offset);
        while (startPos != std::string::npos)
        {
            offset = startPos + keyword.length();
            usize endingSemicolon = source.find(";", offset);
            if (endingSemicolon == std::string::npos)
                return {}; // error will be reported by shader compiler
            usize inputMark = source.find(" in ", offset);
            if (inputMark != std::string::npos && inputMark < endingSemicolon)
            {
                // the attribute is input attribute
                usize textEnd = source.find_last_not_of(" \t\r\n", endingSemicolon);
                usize textStart = source.find_last_of(" \t\r\n", textEnd) + 1;

                attributes.push_back({
                    .TextPosition = textStart,
                    .Name = std::string{source.substr(textStart, textEnd - textStart)}});
            }

            offset = endingSemicolon;
            startPos = source.find(keyword, offset);
        }

        return attributes;
    };

    auto findBindings = [](std::string_view source) -> std::vector<BindingInfo>
    {
        std::vector<BindingInfo> bindings;

        static std::string bindingKeyword = "binding";
        std::string keyword = std::string{META_KEYWORD_PREFIX} + bindingKeyword;

        usize offset = 0;
        usize startPos = source.find(keyword, offset);
        while (startPos != std::string::npos)
        {
            offset = startPos + keyword.length();
            usize bindingNumberStringEnd = std::min(source.find("\r\n", offset), source.find("\n", offset));
            usize bindingNumberStringStart = source.find_last_of(" \t", bindingNumberStringEnd) + 1;

            u32 binding = std::stoul(std::string{source.substr(bindingNumberStringStart,
                bindingNumberStringEnd - bindingNumberStringStart)});
            bindings.push_back({.TextPosition = bindingNumberStringStart, .Binding = binding});

            offset = bindingNumberStringEnd;
            startPos = source.find(keyword, offset);
        }

        return bindings;
    };

    std::vector<AttributeInfo> attributes = findAttributes(shaderSource);
    std::vector<BindingInfo> bindings = findBindings(shaderSource);

    if (attributes.empty())
        return {};

    if (bindings.empty())
    {
        std::vector<InputAttributeBindingInfo> inputAttributeBindingInfos;
        inputAttributeBindingInfos.reserve(attributes.size());
        for (auto& attribute : attributes)
            inputAttributeBindingInfos.push_back({.Binding = 0, .Attribute = attribute.Name});

        return inputAttributeBindingInfos;
    }

    if (attributes.front().TextPosition < bindings.front().TextPosition)
    {
        auto it = std::ranges::find_if(bindings,
            [](auto& bindingInfo){ return bindingInfo.Binding == 0;});
        if (it != bindings.end())
            LOG("WARNING: dangerous implicit binding 0 for some input attributes");

        bindings.insert(bindings.begin(), {.TextPosition = 0, .Binding = 0});
    }

    std::vector<InputAttributeBindingInfo> inputAttributeBindingInfos;
    inputAttributeBindingInfos.reserve(attributes.size());
    
    u32 bindingIndexCurrent = 0;
    u32 bindingIndexNext = bindingIndexCurrent + 1;

    u32 attributeIndexCurrent = 0;

    while (attributeIndexCurrent < attributes.size())
    {
        AttributeInfo& attribute = attributes[attributeIndexCurrent];
        if (bindingIndexNext == bindings.size() || attribute.TextPosition < bindings[bindingIndexNext].TextPosition)
        {
            inputAttributeBindingInfos.push_back({
                .Binding = bindings[bindingIndexCurrent].Binding,
                .Attribute = attribute.Name});
            attributeIndexCurrent++;
        }
        else
        {
            bindingIndexCurrent = bindingIndexNext;
            bindingIndexNext++;
        }
    }

    return inputAttributeBindingInfos;
}

void ShaderConverter::RemoveMetaKeywords(std::string& shaderSource)
{
    std::vector<std::string> keywords = {
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::Dynamic),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::Bindless),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSampler),
        std::string{META_KEYWORD_PREFIX} + assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearest),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampEdge),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampEdge),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampBlack),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampBlack),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerClampWhite),
        std::string{META_KEYWORD_PREFIX} +
            assetLib::descriptorFlagToString(DescriptorFlags::ImmutableSamplerNearestClampWhite),
        std::string{META_KEYWORD_PREFIX} + "binding",
    };

    for (auto keyword : keywords)
    {
        usize pos = shaderSource.find(keyword, 0);
        while (pos != std::string::npos)
        {
            shaderSource.replace(pos, 2, "//");
            pos = shaderSource.find(keyword, pos);
        }
    }
}

assetLib::ShaderInfo ShaderConverter::Reflect(const std::vector<u32>& spirV,
        const std::vector<DescriptorFlagInfo>& flags, const std::vector<InputAttributeBindingInfo>& inputBindings)
{
    static constexpr u32 SPV_INVALID_VAL = (u32)~0;
    
    assetLib::ShaderInfo shaderInfo = {};
    
    SpvReflectShaderModule reflectedModule = {};
    spvReflectCreateShaderModule(spirV.size() * sizeof(u32), spirV.data(), &reflectedModule);

    shaderInfo.ShaderStages = (u32)reflectedModule.shader_stage;

    // extract specialization constants
    u32 specializationCount;
    spvReflectEnumerateSpecializationConstants(&reflectedModule, &specializationCount, nullptr);
    std::vector<SpvReflectSpecializationConstant*> specializationConstants(specializationCount);
    spvReflectEnumerateSpecializationConstants(&reflectedModule, &specializationCount, specializationConstants.data());

    shaderInfo.SpecializationConstants.reserve(specializationCount);
    for (auto& constant : specializationConstants)
        shaderInfo.SpecializationConstants.push_back({
            .Name = constant->name,
            .Id = constant->constant_id,
            .ShaderStages = (u32)reflectedModule.shader_stage});

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
                .Name = input->name,
                .Location = input->location,
                .Format = (u32)input->format,
                .SizeBytes = input->numeric.scalar.width * std::max(1u, input->numeric.vector.component_count) / 8});
            for (auto& inputBinding : inputBindings)
                if (inputBinding.Attribute == input->name)
                {
                    shaderInfo.InputAttributes.back().Binding = inputBinding.Binding;
                    break;
                }
        }
        std::ranges::sort(shaderInfo.InputAttributes,
            [](const auto& a, const auto& b) { return a.Binding < b.Binding ||
                a.Binding == b.Binding && a.Location < b.Location; });
    }

    // extract push constants
    u32 pushCount;
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, nullptr);
    std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, pushConstants.data());

    shaderInfo.PushConstants.reserve(pushCount);
    for (auto& push : pushConstants)
        shaderInfo.PushConstants.push_back({
            .SizeBytes = push->size,
            .Offset = push->offset,
            .ShaderStages = (u32)reflectedModule.shader_stage});
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
        descriptorSet.Descriptors.resize(set->binding_count);
        for (u32 i = 0; i < set->binding_count; i++)
        {
            descriptorSet.Descriptors[i] = {
                .Name = set->bindings[i]->name,
                .Count = set->bindings[i]->count,
                .Binding = set->bindings[i]->binding,
                .Type = (u32)set->bindings[i]->descriptor_type,
                .ShaderStages = (u32)reflectedModule.shader_stage
            };
            auto it = std::ranges::find_if(flags,
               [&descriptorSet, i](const auto& flag)
               {
                   return flag.DescriptorName == descriptorSet.Descriptors[i].Name;
               });

            if (it != flags.end())
                descriptorSet.Descriptors[i].Flags = it->Flags;
            
            if (descriptorSet.Descriptors[i].Flags & DescriptorFlags::Dynamic)
            {
                if (descriptorSet.Descriptors[i].Type == (u32)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    descriptorSet.Descriptors[i].Type = (u32)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                else if (descriptorSet.Descriptors[i].Type == (u32)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    descriptorSet.Descriptors[i].Type = (u32)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }
        }
        std::ranges::sort(descriptorSet.Descriptors,
                          [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
    }
    std::ranges::sort(shaderInfo.DescriptorSets,
                      [](const auto& a, const auto& b) { return a.Set < b.Set; });

    return shaderInfo;
}
