#include "Converters.h"

#include "AssetLib.h"
#include "TextureAsset.h"
#include "SceneAsset.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NOEXCEPTION 
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <mikktspace.h>

#include "utils.h"
#include "core.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stack>
#include <execution>

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
        while (!std::filesystem::equivalent(currentPath, initialDirectoryPath))
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
    textureInfo.OriginalFile = std::filesystem::weakly_canonical(path).generic_string();
    textureInfo.BlobFile = std::filesystem::weakly_canonical(blobPath).generic_string();
    
    assetLib::File textureFile = assetLib::packTexture(textureInfo, pixels);
        
    assetLib::saveAssetFile(assetPath.string(), blobPath.string(), textureFile);

    stbi_image_free(pixels);

    std::cout << std::format("Texture file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}

bool SceneConverter::NeedsConversion(const std::filesystem::path& initialDirectoryPath,
    const std::filesystem::path& path)
{
    return needsConversion(initialDirectoryPath, path, [](std::filesystem::path& converted)
    {
        converted.replace_extension(SceneConverter::POST_CONVERT_EXTENSION);
    });
}

namespace glm
{
    void to_json(nlohmann::json& j, const glm::vec4& vec)
    {
        j = { { "r", vec.x }, { "g", vec.y }, { "b", vec.z }, { "a", vec.w } };
    }
    
    void from_json(const nlohmann::json& j, glm::vec4& vec)
    {
        vec.x = j.at("r").get<f32>();
        vec.y = j.at("g").get<f32>();
        vec.z = j.at("b").get<f32>();
        vec.w = j.at("a").get<f32>();
    }

    void to_json(nlohmann::json& j, const glm::vec3& vec)
    {
        j = { { "x", vec.x }, { "y", vec.y }, { "z", vec.z } };
    }
    
    void from_json(const nlohmann::json& j, glm::vec3& vec)
    {
        vec.x = j.at("x").get<f32>();
        vec.y = j.at("y").get<f32>();
        vec.z = j.at("z").get<f32>();
    }
}

namespace
{
    struct WriteResult
    {
        u64 Offset{};
    };
    template <usize Alignment>
    WriteResult writeAligned(std::vector<u8>& destination, const void* data, u64 size)
    {
        static_assert((Alignment & (Alignment - 1)) == 0);

        const char zeros[Alignment]{};
        const u64 pos = destination.size();
        const u64 padding = (Alignment - (pos & (Alignment - 1))) & (Alignment - 1);
        destination.resize(destination.size() + padding + size);
        memcpy(destination.data() + pos, zeros, padding);
        memcpy(destination.data() + pos + padding, data, size);

        return {.Offset = pos + padding};
    }

    template <typename T>
    struct AccessorDataTypeTraits
    {
        static_assert(sizeof(T) == 0, "No match for type");
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec4>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC4;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec3>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC3;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<glm::vec2>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_VEC2;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT;
    };
    template <>
    struct AccessorDataTypeTraits<u8>
    {
        static constexpr i32 TYPE = TINYGLTF_TYPE_SCALAR;
        static constexpr i32 COMPONENT_TYPE = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    };
    
    struct SceneProcessContext
    {
        tinygltf::Model BakedScene{};

        template <typename T>
        struct AccessorProxy
        {
            u32 ViewIndex{0};
            std::vector<T> Data;
        };
        AccessorProxy<glm::vec3> Positions;
        AccessorProxy<glm::vec3> Normals;
        AccessorProxy<glm::vec4> Tangents;
        AccessorProxy<glm::vec2> UVs;
        AccessorProxy<assetLib::SceneInfo::IndexType> Indices;
        AccessorProxy<assetLib::SceneInfo::Meshlet> Meshlets;
        
        std::string BinaryName{};
        std::filesystem::path BakedScenePath{};

        void Prepare()
        {
            namespace fs = std::filesystem;
            fs::create_directories(BakedScenePath.parent_path());
            if (fs::exists(BakedScenePath))
                fs::remove(BakedScenePath);
            
            BakedScene.accessors.clear();
            BakedScene.buffers.clear();
            BakedScene.bufferViews.clear();
            BakedScene.meshes.clear();

            BakedScene.buffers.resize(1);
            BakedScene.buffers[0].uri = BinaryName;

            BakedScene.bufferViews.resize((u32)assetLib::SceneInfo::BufferViewType::MaxVal);
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Position].name = "Positions";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Normal].name = "Normals";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Tangent].name = "Tangents";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Uv].name = "UVs";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Index].name = "Indices";
            BakedScene.bufferViews[(u32)assetLib::SceneInfo::BufferViewType::Meshlet].name = "Meshlets";
            for (auto& view : BakedScene.bufferViews)
                view.buffer = 0;

            Positions.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Position;
            Normals.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Normal;
            Tangents.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Tangent;
            UVs.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Uv;
            Indices.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Index;
            Meshlets.ViewIndex = (u32)assetLib::SceneInfo::BufferViewType::Meshlet;
        }

        void Finalize()
        {
            static constexpr u32 ALIGNMENT = 4;

            auto writeAndUpdateView = [this](auto accessorProxy) {
                const u64 sizeBytes = accessorProxy.Data.size() * sizeof(accessorProxy.Data[0]);
                WriteResult write = writeAligned<ALIGNMENT>(BakedScene.buffers[0].data,
                    accessorProxy.Data.data(), sizeBytes);
                BakedScene.bufferViews[accessorProxy.ViewIndex].byteOffset = write.Offset;
                BakedScene.bufferViews[accessorProxy.ViewIndex].byteLength = sizeBytes;
            };
            
            writeAndUpdateView(Positions);
            writeAndUpdateView(Normals);
            writeAndUpdateView(Tangents);
            writeAndUpdateView(UVs);
            writeAndUpdateView(Indices);
            writeAndUpdateView(Meshlets);

            tinygltf::TinyGLTF writer = {};
            writer.SetImageWriter(nullptr, nullptr);
            writer.WriteGltfSceneToFile(&BakedScene, BakedScenePath.string());
        }

        template <typename T>
        tinygltf::Accessor CreateAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
        {
            tinygltf::Accessor accessor {};
            accessor.componentType = AccessorDataTypeTraits<T>::COMPONENT_TYPE;
            accessor.type = AccessorDataTypeTraits<T>::TYPE;
            accessor.count = data.size();
            accessor.bufferView = accessorProxy.ViewIndex;
            accessor.byteOffset = accessorProxy.Data.size() * sizeof(T);
            accessorProxy.Data.append_range(data);

            return accessor;
        }

        template <typename T>
        nlohmann::json CreatePseudoAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
        {
            nlohmann::json accessor = {};
            accessor["count"] = data.size();
            accessor["bufferView"] = accessorProxy.ViewIndex;
            accessor["byteOffset"] = accessorProxy.Data.size() * sizeof(T);
            accessorProxy.Data.append_range(data);

            return accessor;
        }
    };
    
    template <typename T>
    void copyBufferToVector(std::vector<T>& vec, tinygltf::Model& gltf, tinygltf::Accessor& accessor)
    {
        const tinygltf::BufferView& bufferView = gltf.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf.buffers[bufferView.buffer];
        const u64 elementSizeBytes =
            (u64)tinygltf::GetNumComponentsInType(accessor.type) *
            (u64)tinygltf::GetComponentSizeInBytes(accessor.componentType);
        ASSERT(elementSizeBytes == sizeof(T))
        const u64 sizeBytes = elementSizeBytes * accessor.count;
        const u64 offset = bufferView.byteOffset + accessor.byteOffset;
        const u32 stride = accessor.ByteStride(bufferView);

        vec.resize(accessor.count);

        if (stride == elementSizeBytes)
            memcpy(vec.data(), buffer.data.data() + offset, sizeBytes);
        else
            for (u32 i = 0; i < accessor.count; i++)
                memcpy(vec.data() + i * elementSizeBytes,
                    buffer.data.data() + offset + (u64)i * stride,
                    elementSizeBytes);
    }

    void generateTriangleNormals(std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& positions, const std::vector<u32> indices)
    {
        normals.resize(positions.size());
        for (u32 i = 0; i < indices.size(); i += 3)
        {
            const glm::vec3 a = positions[indices[i + 1]] - positions[indices[i + 0]];   
            const glm::vec3 b = positions[indices[i + 2]] - positions[indices[i + 0]];
            const glm::vec3 normal = glm::normalize(glm::cross(a, b));
            normals[indices[i + 0]] = normal;
            normals[indices[i + 1]] = normal;
            normals[indices[i + 2]] = normal;
        }
    }

    void generateTriangleTangents(std::vector<glm::vec4>& tangents,
        const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec2>& uvs, const std::vector<u32>& indices)
    {
        struct GeometryInfo
        {
            const std::vector<glm::vec3>* Positions{};
            const std::vector<glm::vec3>* Normals{};
            const std::vector<glm::vec2>* Uvs{};
            std::vector<glm::vec4>* Tangents{};
            const std::vector<u32>* Indices{};
        };
        GeometryInfo gi = {
            .Positions = &positions,
            .Normals = &normals,
            .Uvs = &uvs,
            .Tangents = &tangents,
            .Indices = &indices};

        tangents.resize(normals.size());
        
        SMikkTSpaceInterface interface = {};
        interface.m_getNumFaces = [](const SMikkTSpaceContext* ctx) {
            return (i32)((GeometryInfo*)ctx->m_pUserData)->Indices->size() / 3;  
        };
        interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const i32){ return 3; };
        interface.m_getPosition = [](const SMikkTSpaceContext* ctx, f32 position[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(position, &(*info->Positions)[index], sizeof(glm::vec3));
        };
        interface.m_getNormal = [](const SMikkTSpaceContext* ctx, f32 normal[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(normal, &(*info->Normals)[index], sizeof(glm::vec3));
        };
        interface.m_getTexCoord = [](const SMikkTSpaceContext* ctx, f32 uv[], const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            memcpy(uv, &(*info->Uvs)[index], sizeof(glm::vec2));
        };
        interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx, const f32 tangent[], const f32 sign,
            const i32 face, const i32 vertex) {
            GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
            const u32 index = (*info->Indices)[face * 3 + vertex];
            
            memcpy(&(*info->Tangents)[index], tangent, sizeof(glm::vec3));
            (*info->Tangents)[index][3] = sign;
        };

        SMikkTSpaceContext context = {};
        context.m_pUserData = &gi;
        context.m_pInterface = &interface;
        genTangSpaceDefault(&context);
    }
    
    void processMesh(SceneProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Mesh& mesh)
    {
        for (auto& primitive : mesh.primitives)
        {
            tinygltf::Mesh backedMesh = mesh;
            backedMesh.primitives = {};
            
            ASSERT(primitive.mode == TINYGLTF_MODE_TRIANGLES)
            
            tinygltf::Accessor& indexAccessor = gltf.accessors[primitive.indices];
            std::vector<u32> indices(indexAccessor.count);
            switch (indexAccessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    {
                        std::vector<u8> rawIndices(indexAccessor.count);
                        copyBufferToVector(rawIndices, gltf, indexAccessor);
                        indices.assign_range(rawIndices);
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        std::vector<u16> rawIndices(indexAccessor.count);
                        copyBufferToVector(rawIndices, gltf, indexAccessor);
                        indices.assign_range(rawIndices);
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    copyBufferToVector(indices, gltf, indexAccessor);
                    break;
                default:
                    ASSERT(false)
            }

            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec4> tangents;
            std::vector<glm::vec2> uvs;
            for (auto& attribute : primitive.attributes)
            {
                tinygltf::Accessor& attributeAccessor = gltf.accessors[attribute.second];
                if (attribute.first == "POSITION")
                    copyBufferToVector(positions, gltf, attributeAccessor);
                else if (attribute.first == "NORMAL")
                    copyBufferToVector(normals, gltf, attributeAccessor);
                else if (attribute.first == "TANGENT")
                    copyBufferToVector(tangents, gltf, attributeAccessor);
                else if (attribute.first == "TEXCOORD_0")
                    copyBufferToVector(uvs, gltf, attributeAccessor);
            }

            ASSERT(!positions.empty(), "The mesh has no positions")

            const bool hasNormals = !normals.empty();
            const bool hasTangents = !tangents.empty();
            const bool hasUVs = !uvs.empty();
            
            if (!hasNormals)
                generateTriangleNormals(normals, positions, indices);
            if (!hasTangents && hasUVs)
                generateTriangleTangents(tangents, positions, normals, uvs, indices);
            if (!hasTangents)
                tangents.resize(positions.size(), glm::vec4{0.0f, 0.0f, 1.0f, 1.0f});
            if (!hasUVs)
                uvs.resize(positions.size(), glm::vec2{0.0});

            utils::Attributes attributes {
                .Positions = &positions,
                .Normals = &normals,
                .Tangents = &tangents,
                .UVs = &uvs};
            utils::remapMesh(attributes, indices);
            auto&& [meshlets, meshletIndices] = utils::createMeshlets(attributes, indices);
            auto&& [sphere, box] = utils::meshBoundingVolumes(meshlets);

            i32 currentAccessorIndex = (i32)ctx.BakedScene.accessors.size();
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(positions, ctx.Positions));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(normals, ctx.Normals));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(tangents, ctx.Tangents));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(uvs, ctx.UVs));
            ctx.BakedScene.accessors.push_back(ctx.CreateAccessor(meshletIndices, ctx.Indices));           
            nlohmann::json meshletsAccessor = ctx.CreatePseudoAccessor(meshlets, ctx.Meshlets);

            tinygltf::Primitive backedPrimitive = primitive;
            nlohmann::json extras = {};
            extras["bounding_box"]["min"] = box.Min;
            extras["bounding_box"]["max"] = box.Max;
            extras["bounding_sphere"]["center"] = sphere.Center;
            extras["bounding_sphere"]["radius"] = sphere.Radius;
            extras["meshlets"] = meshletsAccessor;
            tinygltf::ParseJsonAsValue(&backedPrimitive.extras, extras);

            backedPrimitive.attributes = {
                {"POSITION",    currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Position},
                {"NORMAL",      currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Normal},
                {"TANGENT",     currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Tangent},
                {"TEXCOORD_0",  currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Uv},
            };
            backedPrimitive.indices = (i32)currentAccessorIndex + (u32)assetLib::SceneInfo::BufferViewType::Index;
            
            backedMesh.primitives.push_back(backedPrimitive);
            ctx.BakedScene.meshes.push_back(backedMesh);
        }
    }
}

void SceneConverter::Convert(const std::filesystem::path& initialDirectoryPath, const std::filesystem::path& path)
{
    LOG("Converting scene file {}\n", path.string());
    
    auto&& [assetPath, blobPath] = getAssetsPath(initialDirectoryPath, path,
        [](const std::filesystem::path& processedPath)
        {
            AssetPaths paths;
            paths.AssetPath = paths.BlobPath = processedPath;
            paths.AssetPath.replace_extension(POST_CONVERT_EXTENSION);
            paths.BlobPath = paths.BlobPath.filename();
            paths.BlobPath.replace_extension(BLOB_EXTENSION);

            return paths;
        });
    
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    loader.ShouldPreloadBuffersData(false);
    loader.ShouldPreloadImagesData(false);
    std::string errors;
    std::string warnings;
    bool success = loader.LoadASCIIFromFile(&gltf, &errors, &warnings, path.string());
    success = success && loader.LoadBuffersData(&gltf, &errors, &warnings, path.parent_path().string());
    success = success && loader.LoadImagesData(&gltf, &errors, &warnings, path.parent_path().string());
    
    if (!errors.empty())
        LOG("ERROR: {} ({})", errors, path.string());
    if (!warnings.empty())
        LOG("WARNING: {} ({})", warnings, path.string());
    if (!success)
        return;
    if (gltf.scenes.empty())
        return;

    /* process just one scene for now */
    const u32 sceneIndex = gltf.defaultScene > 0 ? (u32)gltf.defaultScene : 0;
    auto& scene = gltf.scenes[sceneIndex];
    if (scene.nodes.empty())
        return;

    SceneProcessContext ctx = {};
    ctx.BinaryName = blobPath.string();
    ctx.BakedScenePath = assetPath;
    ctx.BakedScene = gltf;
    ctx.Prepare();
    
    for (auto& mesh : gltf.meshes)
        processMesh(ctx, gltf, mesh);

    ctx.Finalize();

    LOG("Scene file {} converted to {} (blob at {})\n",
        path.string(), assetPath.string(), blobPath.string());
}
