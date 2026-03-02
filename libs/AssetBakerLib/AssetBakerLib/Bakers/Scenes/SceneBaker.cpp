#include "SceneBaker.h"

#include "mikktspace.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetBakerLib/utils.h>
#include <AssetBakerLib/Bakers/BakersUtils.h>
#include <AssetBakerLib/Bakers/Images/ImageBaker.h>

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NOEXCEPTION 
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>


#define CHECK_RETURN_IO_ERROR(x, error, ...) \
    ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

static_assert(lux::assetlib::SCENE_UNSET_INDEX == (u32)(-1), "gltf absent nodes have value of -1");

namespace lux::bakers
{
namespace 
{
bool requiresBaking(const std::filesystem::path& path, const std::filesystem::path& bakedPath, const Context& ctx)
{
    namespace fs = std::filesystem;
    if (!fs::exists(bakedPath))
        return true;

    const auto lastBaked = fs::last_write_time(bakedPath);
    if (lastBaked < fs::last_write_time(path))
        return true;
    
    IoResult<assetlib::AssetFile> assetFileRead = ctx.Io->ReadHeader(bakedPath);
    if (!assetFileRead.has_value())
        return true;

    const auto unpackHeader = assetlib::scene::readHeader(*assetFileRead);
    if (!unpackHeader.has_value())
        return true;

    return false; 
}
}

std::filesystem::path SceneBaker::GetBakedPath(const std::filesystem::path& originalFile,
    const SceneBakeSettings&, const Context& ctx)
{
    std::filesystem::path path = getPostBakePath(originalFile, ctx);
    path.replace_extension(POST_BAKE_EXTENSION);

    return path;
}

IoResult<std::filesystem::path> SceneBaker::BakeToFile(const std::filesystem::path& path,
    const SceneBakeSettings& settings, const Context& ctx)
{
    const AssetPaths paths = getPostBakePaths(path, ctx, POST_BAKE_EXTENSION, *ctx.Io);
    if (!requiresBaking(path, paths.HeaderPath, ctx))
        return paths.HeaderPath;

    auto baked = Bake(path, settings, ctx);
    CHECK_RETURN_IO_ERROR(baked.has_value(), baked.error().Code, "{} ({})", baked.error().Message, path.string())

    u64 binarySizeBytes = 0;
    for (auto& buffer : baked->Header.Buffers)
        binarySizeBytes += buffer.SizeBytes;

    auto packedScene = assetlib::scene::pack(*baked, *ctx.Compressor);
    if (!packedScene.has_value())
        return std::unexpected(packedScene.error());

    auto existingAssetId = getBakedAssetId(paths.HeaderPath, *ctx.Io);
    if (existingAssetId.HasValue())
        packedScene->Metadata.AssetId = existingAssetId;

    assetlib::AssetFile assetFile = {
        .Metadata = std::move(packedScene->Metadata),
        .AssetSpecificInfo = std::move(packedScene->AssetSpecificInfo)
    };
    assetFile.IoInfo = {
        .OriginalFile = std::filesystem::weakly_canonical(path).generic_string(),
        .HeaderFile = std::filesystem::weakly_canonical(paths.HeaderPath).generic_string(),
        .BinaryFile = std::filesystem::weakly_canonical(paths.BinaryPath).generic_string(),
        .BinarySizeBytes = binarySizeBytes,
        .BinarySizeBytesCompressed = packedScene->PackedBinaries.size(),
        .BinarySizeBytesChunksCompressed = std::move(packedScene->PackedBinarySizeBytesChunks),
        .CompressionMode = ctx.Compressor->GetName(),
        .CompressionGuid = ctx.Compressor->GetGuid()
    };

    IoResult<void> saveResult = ctx.Io->WriteHeader(assetFile);
    CHECK_RETURN_IO_ERROR(saveResult.has_value(), saveResult.error().Code, "{} ({})",
        saveResult.error().Message, path.string())

    IoResult<u64> binarySaveResult = ctx.Io->WriteBinaryChunk(assetFile, packedScene->PackedBinaries);
    CHECK_RETURN_IO_ERROR(binarySaveResult.has_value(), binarySaveResult.error().Code, "{} ({})",
        binarySaveResult.error().Message, path.string())

    return paths.HeaderPath;
}

namespace
{
struct WriteResult
{
    u64 Offset{};
};
template <usize Alignment>
WriteResult writeAligned(std::vector<std::byte>& destination, const void* data, u64 size)
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
    static constexpr assetlib::SceneAssetAccessorType TYPE = assetlib::SceneAssetAccessorType::Vec4;
    static constexpr assetlib::SceneAssetAccessorComponentType COMPONENT_TYPE =
        assetlib::SceneAssetAccessorComponentType::F32;
};
template <>
struct AccessorDataTypeTraits<glm::vec3>
{
    static constexpr assetlib::SceneAssetAccessorType TYPE = assetlib::SceneAssetAccessorType::Vec3;
    static constexpr assetlib::SceneAssetAccessorComponentType COMPONENT_TYPE =
        assetlib::SceneAssetAccessorComponentType::F32;
};
template <>
struct AccessorDataTypeTraits<glm::vec2>
{
    static constexpr assetlib::SceneAssetAccessorType TYPE = assetlib::SceneAssetAccessorType::Vec2;
    static constexpr assetlib::SceneAssetAccessorComponentType COMPONENT_TYPE =
        assetlib::SceneAssetAccessorComponentType::F32;
};
template <>
struct AccessorDataTypeTraits<u8>
{
    static constexpr assetlib::SceneAssetAccessorType TYPE = assetlib::SceneAssetAccessorType::Scalar;
    static constexpr assetlib::SceneAssetAccessorComponentType COMPONENT_TYPE =
        assetlib::SceneAssetAccessorComponentType::U8;
};
template <>
struct AccessorDataTypeTraits<assetlib::SceneAssetMeshlet>
{
    static constexpr assetlib::SceneAssetAccessorType TYPE = assetlib::SceneAssetAccessorType::Scalar;
    static constexpr assetlib::SceneAssetAccessorComponentType COMPONENT_TYPE =
        assetlib::SceneAssetAccessorComponentType::Meshlet;
};

struct ProcessContext
{
    template <typename T>
    struct AccessorProxy
    {
        u32 ViewIndex{0};
        std::vector<T> Data{};
    };
    AccessorProxy<glm::vec3> Positions{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Position};
    AccessorProxy<glm::vec3> Normals{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Normal};
    AccessorProxy<glm::vec4> Tangents{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Tangent};
    AccessorProxy<glm::vec2> UVs{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Uv};
    AccessorProxy<assetlib::SceneAssetIndexType> Indices{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Index};
    AccessorProxy<assetlib::SceneAssetMeshlet> Meshlets{.ViewIndex = (u32)assetlib::SceneAssetBufferViewType::Meshlet};

    assetlib::SceneAssetHeader SceneAssetHeader{};
    std::vector<std::byte> SceneBufferData{};

    std::filesystem::path ScenePath{};
    std::filesystem::path InitialDirectory{};
    assetlib::io::AssetIoInterface& Io;
    assetlib::io::AssetCompressor& Compressor;

public:
    template <typename T>
    assetlib::SceneAssetAccessor CreateAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
    {
        assetlib::SceneAssetAccessor accessor {};
        accessor.ComponentType = AccessorDataTypeTraits<T>::COMPONENT_TYPE;
        accessor.Type = AccessorDataTypeTraits<T>::TYPE;
        accessor.Count = (u32)data.size();
        accessor.BufferView = accessorProxy.ViewIndex;
        accessor.OffsetBytes = accessorProxy.Data.size() * sizeof(T);
        accessorProxy.Data.append_range(data);

        return accessor;
    }

    void Finalize()
    {
        static constexpr u32 ALIGNMENT = 4;

        SceneAssetHeader.BufferViews.resize((u32)assetlib::SceneAssetBufferViewType::MaxVal);
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Position].Name = "Positions";
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Normal].Name = "Normals";
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Tangent].Name = "Tangents";
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Uv].Name = "Uvs";
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Index].Name = "Indices";
        SceneAssetHeader.BufferViews[(u32)assetlib::SceneAssetBufferViewType::Meshlet].Name = "Meshlet";

        auto writeAndUpdateView = [this](auto accessorProxy) {
            const u64 sizeBytes = accessorProxy.Data.size() * sizeof(accessorProxy.Data[0]);
            WriteResult write = writeAligned<ALIGNMENT>(SceneBufferData, accessorProxy.Data.data(), sizeBytes);
            
            SceneAssetHeader.BufferViews[accessorProxy.ViewIndex].Buffer = 0;
            SceneAssetHeader.BufferViews[accessorProxy.ViewIndex].OffsetBytes = write.Offset;
            SceneAssetHeader.BufferViews[accessorProxy.ViewIndex].LengthBytes = sizeBytes;
        };

        writeAndUpdateView(Positions);
        writeAndUpdateView(Normals);
        writeAndUpdateView(Tangents);
        writeAndUpdateView(UVs);
        writeAndUpdateView(Indices);
        writeAndUpdateView(Meshlets);
        SceneAssetHeader.Buffers.push_back({.SizeBytes = SceneBufferData.size()});
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
        .Indices = &indices
    };

    tangents.resize(normals.size());

    SMikkTSpaceInterface interface = {};
    interface.m_getNumFaces = [](const SMikkTSpaceContext* ctx)
    {
        return (i32)((GeometryInfo*)ctx->m_pUserData)->Indices->size() / 3;
    };
    interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const i32) { return 3; };
    interface.m_getPosition = [](const SMikkTSpaceContext* ctx, f32 position[], const i32 face, const i32 vertex)
    {
        GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
        const u32 index = (*info->Indices)[face * 3 + vertex];
        memcpy(position, &(*info->Positions)[index], sizeof(glm::vec3));
    };
    interface.m_getNormal = [](const SMikkTSpaceContext* ctx, f32 normal[], const i32 face, const i32 vertex)
    {
        GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
        const u32 index = (*info->Indices)[face * 3 + vertex];
        memcpy(normal, &(*info->Normals)[index], sizeof(glm::vec3));
    };
    interface.m_getTexCoord = [](const SMikkTSpaceContext* ctx, f32 uv[], const i32 face, const i32 vertex)
    {
        GeometryInfo* info = (GeometryInfo*)ctx->m_pUserData;
        const u32 index = (*info->Indices)[face * 3 + vertex];
        memcpy(uv, &(*info->Uvs)[index], sizeof(glm::vec2));
    };
    interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx, const f32 tangent[], const f32 sign,
        const i32 face, const i32 vertex)
        {
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

IoResult<void> processMesh(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Mesh& mesh)
{
    for (auto& primitive : mesh.primitives)
    {
        CHECK_RETURN_IO_ERROR(primitive.mode == TINYGLTF_MODE_TRIANGLES, IoError::ErrorCode::GeneralError,
            "Failed to process mesh {}. The mesh mode is not triangles", mesh.name)
        
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
            return std::unexpected(assetlib::io::IoError{
                .Code = IoError::ErrorCode::WrongFormat,
                .Message = std::format("Unexpected index accessor type: {}", indexAccessor.componentType)
            });
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> uvs;
        for (auto& attribute : primitive.attributes)
        {
            tinygltf::Accessor& attributeAccessor = gltf.accessors[attribute.second];
            if (attribute.first == assetlib::SceneAssetPrimitive::ATTRIBUTE_POSITION_NAME)
                copyBufferToVector(positions, gltf, attributeAccessor);
            else if (attribute.first == assetlib::SceneAssetPrimitive::ATTRIBUTE_NORMAL_NAME)
                copyBufferToVector(normals, gltf, attributeAccessor);
            else if (attribute.first == assetlib::SceneAssetPrimitive::ATTRIBUTE_TANGENT_NAME)
                copyBufferToVector(tangents, gltf, attributeAccessor);
            else if (attribute.first == assetlib::SceneAssetPrimitive::ATTRIBUTE_UV0_NAME)
                copyBufferToVector(uvs, gltf, attributeAccessor);
        }
        CHECK_RETURN_IO_ERROR(!positions.empty(), IoError::ErrorCode::GeneralError,
            "Failed to process mesh {}. The mesh has no positions data", mesh.name)

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
            .UVs = &uvs
        };
        utils::remapMesh(attributes, indices);
        auto&& [meshlets, meshletIndices] = utils::createMeshlets(attributes, indices);
        auto&& [sphere, box] = utils::meshBoundingVolumes(meshlets);

        u32 lastAccessor = (u32)ctx.SceneAssetHeader.Accessors.size();
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(positions, ctx.Positions));
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(normals, ctx.Normals));
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(tangents, ctx.Tangents));
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(uvs, ctx.UVs));
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(meshletIndices, ctx.Indices));
        ctx.SceneAssetHeader.Accessors.push_back(ctx.CreateAccessor(meshlets, ctx.Meshlets));

        assetlib::SceneAssetPrimitive bakedPrimitive = {
            .Attributes = {
                assetlib::SceneAssetPrimitive::Attribute{
                    .Name = std::string(assetlib::SceneAssetPrimitive::ATTRIBUTE_POSITION_NAME),
                    .Accessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Position
                },
                assetlib::SceneAssetPrimitive::Attribute{
                    .Name = std::string(assetlib::SceneAssetPrimitive::ATTRIBUTE_NORMAL_NAME),
                    .Accessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Normal
                },
                assetlib::SceneAssetPrimitive::Attribute{
                    .Name = std::string(assetlib::SceneAssetPrimitive::ATTRIBUTE_TANGENT_NAME),
                    .Accessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Tangent
                },
                assetlib::SceneAssetPrimitive::Attribute{
                    .Name = std::string(assetlib::SceneAssetPrimitive::ATTRIBUTE_UV0_NAME),
                    .Accessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Uv
                },
                assetlib::SceneAssetPrimitive::Attribute{
                    .Name = std::string(assetlib::SceneAssetPrimitive::ATTRIBUTE_MESHLET_NAME),
                    .Accessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Meshlet
                },
            },
            .Material = (u32)primitive.material,
            .IndicesAccessor = lastAccessor + (u32)assetlib::SceneAssetBufferViewType::Index,
            .BoundingSphere = sphere,
            .BoundingBox = box, 
        };
        ctx.SceneAssetHeader.Meshes.push_back({
            .Primitives = {bakedPrimitive}
        });
    }

    return {};
}

/* this function decodes percent encoding chars in uri */
std::string decodeUri(const std::string& uri)
{
    std::string decoded;
    decoded.reserve(uri.size());

    static constexpr auto LUT = []() consteval {
        std::array<char, std::numeric_limits<char>::max()> array{};
        array.fill(~0);
        array['0'] = 0x0; array['1'] = 0x1; array['2'] = 0x2;
        array['3'] = 0x3; array['4'] = 0x4; array['5'] = 0x5;
        array['6'] = 0x6; array['7'] = 0x7; array['8'] = 0x8;
        array['9'] = 0x9; array['a'] = 0xa; array['b'] = 0xb;
        array['c'] = 0xc; array['d'] = 0xd; array['e'] = 0xe;
        array['f'] = 0xf; array['A'] = 0xa; array['B'] = 0xb;
        array['C'] = 0xc; array['D'] = 0xd; array['E'] = 0xe;
        array['F'] = 0xf;

        return array;
    }();
    
    for (u32 i = 0; i < uri.size(); i++)
    {
        const char c = uri[i];
        switch (c)
        {
        case '%':
            {
                if (i + 2 >= uri.size())
                {
                    decoded.push_back(c);
                    break;
                }
                const char cFirst = LUT[uri[i + 1]];
                const char cSecond = LUT[uri[i + 2]];
                const char byte = (char)(cFirst << 4 | cSecond);
                decoded.push_back(byte);
                i += 2;
            }
            break;
        case '+':
            decoded.push_back(' ');
            break;
        default:
            decoded.push_back(c);
            break;
        }
    }
    decoded.shrink_to_fit();
    
    return decoded;
}

IoResult<assetlib::AssetId> bakeMaterialImage(ProcessContext& ctx, tinygltf::Model& gltf, i32 textureIndex,
    assetlib::ImageFormat imageFormat)
{
    if (textureIndex < 0)
        return assetlib::AssetId::CreateEmpty();

    const auto& texture = gltf.textures[textureIndex];
    if (texture.source < 0)
        return assetlib::AssetId::CreateEmpty();

    const tinygltf::Image& image = gltf.images[texture.source];

    CHECK_RETURN_IO_ERROR(image.bufferView == -1, IoError::ErrorCode::GeneralError,
        "Failed to bake: image must be external", image.name)

    const std::filesystem::path imagePath = ctx.ScenePath.parent_path() / decodeUri(image.uri);
    ImageBaker baker = {};
    auto bakedImagePath = baker.BakeToFile(imagePath, {.BakedFormat = imageFormat}, {
        .InitialDirectory = ctx.InitialDirectory,
        .Io = &ctx.Io,
        .Compressor = &ctx.Compressor
    });
        
    CHECK_RETURN_IO_ERROR(bakedImagePath.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake image {}", image.uri)

    LUX_LOG_INFO("Baked image file: {}, for scene {}", imagePath.string(), ctx.ScenePath.string());

    auto imageAsset = ctx.Io.ReadHeader(*bakedImagePath);
    CHECK_RETURN_IO_ERROR(bakedImagePath.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to read baked image {}", image.uri)

    return imageAsset->Metadata.AssetId;
}

IoResult<assetlib::AssetId> bakeMaterial(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Material& material)
{
    assetlib::MaterialAlphaMode alphaMode = assetlib::MaterialAlphaMode::Opaque;
    if (material.alphaMode == "MASK")
        alphaMode = assetlib::MaterialAlphaMode::Mask;
    else if (material.alphaMode == "BLEND")
        alphaMode = assetlib::MaterialAlphaMode::Translucent;

    auto bakedBaseColor = bakeMaterialImage(ctx, gltf, material.pbrMetallicRoughness.baseColorTexture.index,
        assetlib::ImageFormat::RGBA8_SRGB);
    CHECK_RETURN_IO_ERROR(bakedBaseColor.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", bakedBaseColor.error(), material.name)
    
    auto bakedEmissive = bakeMaterialImage(ctx, gltf, material.emissiveTexture.index,
        assetlib::ImageFormat::RGBA8_SRGB);
    CHECK_RETURN_IO_ERROR(bakedEmissive.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", bakedEmissive.error(), material.name)
    
    auto bakedNormal = bakeMaterialImage(ctx, gltf, material.normalTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IO_ERROR(bakedNormal.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", bakedNormal.error(), material.name)
    
    auto bakedMetallicRoughness = bakeMaterialImage(ctx, gltf,
        material.pbrMetallicRoughness.metallicRoughnessTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IO_ERROR(bakedMetallicRoughness.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", bakedMetallicRoughness.error(), material.name)
    
    auto bakedOcclusion = bakeMaterialImage(ctx, gltf,
        material.occlusionTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IO_ERROR(bakedOcclusion.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", bakedOcclusion.error(), material.name)

    const assetlib::MaterialAsset bakedMaterial = {
        .Name = material.name,
        .BaseColor = glm::vec4{*(glm::dvec4*)material.pbrMetallicRoughness.baseColorFactor.data()},
        .Metallic = (f32)material.pbrMetallicRoughness.metallicFactor,
        .Roughness = (f32)material.pbrMetallicRoughness.roughnessFactor,
        .EmissiveFactor = glm::vec3{*(glm::dvec3*)material.pbrMetallicRoughness.baseColorFactor.data()},
        .AlphaMode = alphaMode,
        .AlphaCutoff = (f32)material.alphaCutoff,
        .DoubleSided = material.doubleSided,
        .OcclusionStrength = (f32)material.occlusionTexture.strength,
        .BaseColorTexture = *bakedBaseColor,
        .EmissiveTexture = *bakedEmissive,
        .NormalTexture = *bakedNormal,
        .MetallicRoughnessTexture = *bakedMetallicRoughness,
        .OcclusionTexture = *bakedOcclusion
    };

    std::filesystem::path bakedMaterialPath = ctx.ScenePath.parent_path() /
        (material.name + std::string(MATERIAL_ASSET_EXTENSION));

    auto packedMaterial = assetlib::material::pack(bakedMaterial);
    if (!packedMaterial.has_value())
        return std::unexpected(packedMaterial.error());

    auto existingAssetId = getBakedAssetId(bakedMaterialPath, ctx.Io);
    if (existingAssetId.HasValue())
        packedMaterial->Metadata.AssetId = existingAssetId;
    
    assetlib::AssetFile assetFile = {
        .Metadata = std::move(packedMaterial->Metadata),
        .AssetSpecificInfo = std::move(packedMaterial->AssetSpecificInfo)
    };
    assetFile.IoInfo = {
        .OriginalFile = std::filesystem::weakly_canonical(bakedMaterialPath).generic_string(),
        .HeaderFile = std::filesystem::weakly_canonical(bakedMaterialPath).generic_string(),
        .BinaryFile = std::filesystem::weakly_canonical(bakedMaterialPath).generic_string(),
        .CompressionMode = ctx.Compressor.GetName(),
        .CompressionGuid = ctx.Compressor.GetGuid()
    };

    IoResult<void> saveResult = ctx.Io.WriteHeader(assetFile);
    CHECK_RETURN_IO_ERROR(bakedOcclusion.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to bake material: {} ({})", saveResult.error(), material.name)

    return assetFile.Metadata.AssetId;
}

IoResult<void> processMaterial(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Material& material)
{
    auto convertSampleInfo = [&](i32 textureIndex, i32 uvIndex) -> assetlib::SceneAssetTextureSample {
        if (textureIndex < 0)
            return {};

        const auto& texture = gltf.textures[textureIndex];
        const auto& sampler = gltf.samplers[texture.sampler];

        auto filter = assetlib::SceneAssetTextureFilter::Linear;
        switch (sampler.minFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            filter = assetlib::SceneAssetTextureFilter::Nearest;
            break;
        default:
            break;
        }
        
        return {
            .UvIndex = (u32)uvIndex,
            .Filter = filter
        };
    };

    auto bakedMaterial = bakeMaterial(ctx, gltf, material);
    if (!bakedMaterial)
        return std::unexpected(bakedMaterial.error());

    ctx.SceneAssetHeader.Materials.push_back({
        .Name = material.name,
        .MaterialAsset = *bakedMaterial,
        .BaseColorSample = convertSampleInfo(
            material.pbrMetallicRoughness.baseColorTexture.index,
            material.pbrMetallicRoughness.baseColorTexture.texCoord),
        .EmissiveSample = convertSampleInfo(
            material.emissiveTexture.index,
            material.emissiveTexture.texCoord),
        .NormalSample = convertSampleInfo(
            material.normalTexture.index,
            material.normalTexture.texCoord),
        .MetallicRoughnessSample = convertSampleInfo(
            material.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord),
        .OcclusionSample = convertSampleInfo(
            material.occlusionTexture.index,
            material.occlusionTexture.texCoord),
    });

    return {};
}

void processCamera(ProcessContext& ctx, tinygltf::Model&, tinygltf::Camera& camera)
{
    if (camera.type == "perspective")
        ctx.SceneAssetHeader.Cameras.push_back({
            .Type = assetlib::SceneAssetCameraType::Perspective,
            .Near = (f32)camera.perspective.znear,
            .Far = (f32)camera.perspective.zfar,
            .Perspective = assetlib::SceneAssetCamera::PerspectiveData{
                .Aspect = (f32)camera.perspective.aspectRatio,
                .FovY = (f32)camera.perspective.yfov
            },
        });
    else
        ctx.SceneAssetHeader.Cameras.push_back({
            .Type = assetlib::SceneAssetCameraType::Orthographic,
            .Near = (f32)camera.orthographic.znear,
            .Far = (f32)camera.orthographic.zfar,
            .Orthographic = assetlib::SceneAssetCamera::OrthographicData{
                .SpanX = (f32)camera.orthographic.xmag,
                .SpanY = (f32)camera.orthographic.ymag
            },
        });
}

void processLight(ProcessContext& ctx, tinygltf::Model&, tinygltf::Light& light)
{
    auto lightType = assetlib::SceneAssetLightType::Point;
    if (light.type == "directional")
        lightType = assetlib::SceneAssetLightType::Directional;
    if (light.type == "spot")
        lightType = assetlib::SceneAssetLightType::Spot;

    static constexpr f32 UNLIMITED_RANGE = 1e+5f;
    
    ctx.SceneAssetHeader.Lights.push_back({
        .Type = lightType,
        .Color = (glm::vec3)*(glm::dvec3*)light.color.data(),
        .Intensity = (f32)light.intensity,
        .Range = light.range > 0 ? (f32)light.range : UNLIMITED_RANGE
    });
}

void processNode(ProcessContext& ctx, tinygltf::Model&, tinygltf::Node& node)
{
    auto getTransform = [](tinygltf::Node& node) {
        glm::dvec3 translation{0.0f};
        glm::dquat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::dvec3 scale{1.0f};
        
        if (!node.matrix.empty())
        {
            glm::dvec3 scew;
            glm::dvec4 perspective;
            glm::decompose(*(glm::dmat4*)node.matrix.data(), scale, rotation, translation, scew, perspective);
        }
        else
        {
            if (!node.translation.empty())
                translation = *(glm::dvec3*)node.translation.data();
            if (!node.rotation.empty())
            {
                rotation.x = (f32)node.rotation[0];
                rotation.y = (f32)node.rotation[1];
                rotation.z = (f32)node.rotation[2];
                rotation.w = (f32)node.rotation[3];
            }
            if (!node.scale.empty())
                scale = *(glm::dvec3*)node.scale.data();
        }

        return Transform3d{
            .Position = translation,
            .Orientation = rotation,
            .Scale = scale
        };
    };
    
    assetlib::SceneAssetNode bakedNode = {
        .Name = node.name,
        .Camera = (u32)node.camera,
        .Light = (u32)node.light,
        .Mesh = (u32)node.mesh,
        .Transform = getTransform(node)      
    };
    bakedNode.Children.reserve(node.children.size());
    for (i32 child : node.children)
        bakedNode.Children.push_back((u32)child);
    
    ctx.SceneAssetHeader.Nodes.push_back(std::move(bakedNode));
}

void processSubscene(ProcessContext& ctx, tinygltf::Model&, tinygltf::Scene& subscene)
{
    assetlib::SceneAssetSubscene backedSubscene = {
        .Name = subscene.name,
    };
    backedSubscene.Nodes.reserve(subscene.nodes.size());
    for (i32 node : subscene.nodes)
        backedSubscene.Nodes.push_back(node);

    ctx.SceneAssetHeader.Subscenes.push_back(std::move(backedSubscene));
}
}

IoResult<assetlib::SceneAsset> SceneBaker::Bake(const std::filesystem::path& path, const SceneBakeSettings& settings,
    const Context& ctx)
{
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;

    std::string errors;
    std::string warnings;
    const bool success = loader.LoadASCIIFromFile(&gltf, &errors, &warnings, path.string());

    CHECK_RETURN_IO_ERROR(errors.empty() && success, IoError::ErrorCode::GeneralError,
        "Failed to bake scene: {} ({})", errors, path.string())

    /* process just one scene for now */
    const u32 sceneIndex = gltf.defaultScene > 0 ? (u32)gltf.defaultScene : 0;
    auto& scene = gltf.scenes[sceneIndex];
    CHECK_RETURN_IO_ERROR(!scene.nodes.empty(), IoError::ErrorCode::GeneralError,
        "Failed to bake scene: no nodes in default scene")

    ProcessContext processCtx = {
        .ScenePath = path,
        .InitialDirectory = ctx.InitialDirectory,
        .Io = *ctx.Io,
        .Compressor = *ctx.Compressor 
    };
    
    for (auto& material : gltf.materials)
    {
        auto materialProcessResult = processMaterial(processCtx, gltf, material);
        CHECK_RETURN_IO_ERROR(materialProcessResult.has_value(), IoError::ErrorCode::GeneralError,
            "Failed to bake scene: {} ({})", materialProcessResult.error(), path.string())
    }
    for (auto& mesh : gltf.meshes)
    {
        auto meshProcessResult = processMesh(processCtx, gltf, mesh);
        CHECK_RETURN_IO_ERROR(meshProcessResult.has_value(), IoError::ErrorCode::GeneralError,
            "Failed to bake scene: {} ({})", meshProcessResult.error(), path.string())
    }

    for (auto& camera : gltf.cameras)
        processCamera(processCtx, gltf, camera);
    
    for (auto& light : gltf.lights)
        processLight(processCtx, gltf, light);

    for (auto& node : gltf.nodes)
        processNode(processCtx, gltf, node);

    processSubscene(processCtx, gltf, scene);
    processCtx.SceneAssetHeader.DefaultSubscene = sceneIndex;

    processCtx.Finalize();

    return assetlib::SceneAsset{
        .Header = std::move(processCtx.SceneAssetHeader),
        .BuffersData = {std::move(processCtx.SceneBufferData)}
    };
}

bool SceneBaker::ShouldBake(const std::filesystem::path& path, const SceneBakeSettings& settings, const Context& ctx)
{
    const AssetPaths paths = getPostBakePaths(path, ctx, POST_BAKE_EXTENSION, *ctx.Io);
    
    return requiresBaking(path, paths.HeaderPath, ctx);
}
}
