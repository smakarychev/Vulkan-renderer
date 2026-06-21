#include "SceneBaker.h"

#include "SceneUtils.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Scenes/Scene/SceneMeta.h>
#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferAsset.h>
#include <AssetLib/Scenes/Mesh/MeshAsset.h>
#include <AssetImportLib/Bakers/BakersUtils.h>
#include <AssetImportLib/Bakers/Images/ImageBaker.h>
#include <AssetImportLib/Importers/Images/ImageImporter.h>
#include <AssetImportLib/Importers/Scenes/GeometryBufferImporter.h>
#include <AssetImportLib/Importers/Scenes/MeshImporter.h>
#include <AssetImportLib/Importers/Scenes/SceneImporter.h>
#include <AssetImportLib/Importers/Materials/MaterialImporter.h>
#include <CoreLib/Utils/FileUtils.h>

#include <mikktspace.h>

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

#define CHECK_RETURN_IO_ERROR_PROPAGATE(result) \
ASSETLIB_CHECK_RETURN_IO_ERROR_PROPAGATE(result)

static_assert(lux::assetlib::SCENE_UNSET_INDEX == (u32)(-1), "gltf absent nodes have value of -1");

namespace lux::import
{
IoResult<std::filesystem::path> SceneBaker::BakeToFile(assetlib::SceneMeta& meta, 
    const std::filesystem::path& metaPath)
{
    ASSERT(!meta.Metadata.Io.OriginalFile.empty())

    const AssetPaths paths = getPostBakePaths(meta.Metadata, SCENE_ASSET_EXTENSION, *m_Ctx);
    auto baked = Bake(meta);
    CHECK_RETURN_IO_ERROR_PROPAGATE(baked)
    
    auto packedScene = assetlib::scene::pack(*baked);
    CHECK_RETURN_IO_ERROR_PROPAGATE(packedScene)
    
    IoResult<u64> saveResult = m_Ctx->Io->WriteHeader(meta.Metadata, packedScene->Header);
    CHECK_RETURN_IO_ERROR_PROPAGATE(saveResult)

    meta.Metadata.Io = {
        .OriginalFile = meta.Metadata.Io.OriginalFile,
        .HeaderFile = meta.Metadata.Io.HeaderFile,
        .BinaryFile = meta.Metadata.Io.BinaryFile,
        .HeaderSizeBytes = *saveResult,
        .IoMode = m_Ctx->Io->GetName(),
        .CompressionMode = m_Ctx->Compressor->GetName(),
        .IoGuid = m_Ctx->Io->GetGuid(),
        .CompressionGuid = m_Ctx->Compressor->GetGuid()
    };
    CHECK_RETURN_IO_ERROR_PROPAGATE(Importer::UpdatePackedMetadataSilent(
        metaPath, assetlib::scene::packMeta(meta), "scene"))

    return paths.HeaderPath;
}

bool SceneBaker::NeedsBaking(const std::filesystem::path& metaPath) const
{
    namespace fs = std::filesystem;

    ASSERT(metaPath.extension().string() == assetlib::ASSETLIB_METADATA_EXTENSION)

    auto metaRead = assetlib::scene::readMeta(metaPath);
    if (!metaRead.has_value())
        return true;

    const std::filesystem::path rawPath = metaRead->Metadata.Io.OriginalFile;
    const std::filesystem::path bakedPath = getPostBakePath(metaRead->Metadata, SCENE_ASSET_EXTENSION, *m_Ctx);

    if (!fs::exists(bakedPath))
        return true;

    const auto lastBaked = fs::last_write_time(bakedPath);
    if (lastBaked < fs::last_write_time(metaPath) || lastBaked < fs::last_write_time(rawPath))
        return true;

    const auto readHeader = assetlib::scene::readScene(metaRead->Metadata);
    if (!readHeader.has_value())
        return true;

    return false;
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
struct AccessorDataTypeTraits<glm::u16vec4>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Vec4;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::U16;
};

template <>
struct AccessorDataTypeTraits<glm::vec4>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Vec4;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::F32;
};

template <>
struct AccessorDataTypeTraits<glm::vec3>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Vec3;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::F32;
};

template <>
struct AccessorDataTypeTraits<glm::vec2>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Vec2;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::F32;
};

template <>
struct AccessorDataTypeTraits<u8>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Scalar;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::U8;
};

template <>
struct AccessorDataTypeTraits<f32>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Scalar;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::F32;
};

template <>
struct AccessorDataTypeTraits<assetlib::SceneAssetMeshlet>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Scalar;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::Meshlet;
};

template <>
struct AccessorDataTypeTraits<glm::mat4>
{
    static constexpr assetlib::GeometryBufferAccessorType TYPE = assetlib::GeometryBufferAccessorType::Mat4;
    static constexpr assetlib::GeometryBufferAccessorComponentType COMPONENT_TYPE =
        assetlib::GeometryBufferAccessorComponentType::F32;
};

struct ProcessContext
{
    template <typename T>
    struct AccessorProxy
    {
        u32 ViewIndex{0};
        std::vector<T> Data{};
    };

    AccessorProxy<glm::vec3> Positions{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Position};
    AccessorProxy<glm::vec3> Normals{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Normal};
    AccessorProxy<glm::vec4> Tangents{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Tangent};
    AccessorProxy<glm::vec2> UVs{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Uv};
    AccessorProxy<glm::u16vec4> Joints{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Joint};
    AccessorProxy<glm::vec4> Weights{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Weight};
    AccessorProxy<assetlib::SceneAssetIndexType> Indices{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Index};
    AccessorProxy<assetlib::SceneAssetMeshlet> Meshlets{.ViewIndex = (u32)assetlib::GeometryBufferViewType::Meshlet};
    AccessorProxy<glm::mat4> InverseBindMatrices{
        .ViewIndex = (u32)assetlib::GeometryBufferViewType::InverseBindMatrix
    };
    AccessorProxy<f32> AnimationTimestamps{.ViewIndex = (u32)assetlib::GeometryBufferViewType::AnimationTimestamp};
    AccessorProxy<glm::vec3> AnimationPositionKeyframes{
        .ViewIndex = (u32)assetlib::GeometryBufferViewType::AnimationPositionKeyframe
    };
    AccessorProxy<glm::vec4> AnimationOrientationKeyframes{
        .ViewIndex = (u32)assetlib::GeometryBufferViewType::AnimationOrientationKeyframe
    };
    AccessorProxy<glm::vec3> AnimationScaleKeyframes{
        .ViewIndex = (u32)assetlib::GeometryBufferViewType::AnimationScaleKeyframe
    };

    assetlib::SceneAsset SceneAsset{};
    assetlib::GeometryBufferAsset GeometryBufferAsset{};
    assetlib::AssetId GeometryBufferAssetId{};
    struct MeshInfo
    {
        assetlib::MeshAsset Asset{};
        assetlib::AssetId AssetId{};
        std::string Name{};
    };
    std::vector<MeshInfo> MeshAssets{};
    std::vector<assetlib::MeshPrimitiveMaterial> Materials{};

    std::filesystem::path ScenePath{};
    std::string BufferUri{};
    u32 BufferHash{};
    std::shared_ptr<Context> Ctx{nullptr};
public:
    template <typename T>
    assetlib::GeometryBufferAccessor CreateAccessor(const std::vector<T>& data, AccessorProxy<T>& accessorProxy)
    {
        assetlib::GeometryBufferAccessor accessor{};
        accessor.ComponentType = AccessorDataTypeTraits<T>::COMPONENT_TYPE;
        accessor.Type = AccessorDataTypeTraits<T>::TYPE;
        accessor.Count = (u32)data.size();
        accessor.BufferView = accessorProxy.ViewIndex;
        accessor.OffsetBytes = accessorProxy.Data.size() * sizeof(T);
        accessorProxy.Data.append_range(data);

        return accessor;
    }

    void FinalizeGeometry()
    {
        static constexpr u32 ALIGNMENT = 4;

        auto& geometryBufferHeader = GeometryBufferAsset.Header;
        geometryBufferHeader.BufferViews.resize((u32)assetlib::GeometryBufferViewType::MaxVal);
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Position].Name = "Positions";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Normal].Name = "Normals";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Tangent].Name = "Tangents";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Uv].Name = "Uvs";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Joint].Name = "Joints";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Weight].Name = "Weights";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Index].Name = "Indices";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::Meshlet].Name = "Meshlets";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::InverseBindMatrix].Name = 
            "InverseBindMatrices";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::AnimationTimestamp].Name = 
            "AnimationTimestamps";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::AnimationPositionKeyframe].Name = 
            "AnimationKeyframePositions";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::AnimationOrientationKeyframe].Name = 
            "AnimationKeyframeOrientations";
        geometryBufferHeader.BufferViews[(u32)assetlib::GeometryBufferViewType::AnimationScaleKeyframe].Name = 
            "AnimationKeyframeScales";

        auto writeAndUpdateView = [&geometryBufferHeader, this](auto accessorProxy) {
            const u64 sizeBytes = accessorProxy.Data.size() * sizeof(accessorProxy.Data[0]);
            WriteResult write = 
                writeAligned<ALIGNMENT>(GeometryBufferAsset.Data, accessorProxy.Data.data(), sizeBytes);

            geometryBufferHeader.BufferViews[accessorProxy.ViewIndex].Buffer = 0;
            geometryBufferHeader.BufferViews[accessorProxy.ViewIndex].OffsetBytes = write.Offset;
            geometryBufferHeader.BufferViews[accessorProxy.ViewIndex].LengthBytes = sizeBytes;
        };

        writeAndUpdateView(Positions);
        writeAndUpdateView(Normals);
        writeAndUpdateView(Tangents);
        writeAndUpdateView(UVs);
        writeAndUpdateView(Joints);
        writeAndUpdateView(Weights);
        writeAndUpdateView(Indices);
        writeAndUpdateView(Meshlets);
        writeAndUpdateView(InverseBindMatrices);
        writeAndUpdateView(AnimationTimestamps);
        writeAndUpdateView(AnimationPositionKeyframes);
        writeAndUpdateView(AnimationOrientationKeyframes);
        writeAndUpdateView(AnimationScaleKeyframes);
        
        GeometryBufferAsset.Header.SizeBytes = GeometryBufferAsset.Data.size();
    }
};

/* this function decodes percent encoding chars in uri */
std::string decodeUri(const std::string& uri)
{
    std::string decoded;
    decoded.reserve(uri.size());

    static constexpr auto LUT = []() consteval
    {
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

IoResult<assetlib::AssetId> importMaterialImage(ProcessContext& ctx, tinygltf::Model& gltf, i32 textureIndex,
    assetlib::ImageFormat imageFormat)
{
    if (textureIndex < 0)
        return assetlib::AssetId::CreateEmpty();

    const auto& texture = gltf.textures[textureIndex];
    if (texture.source < 0)
        return assetlib::AssetId::CreateEmpty();

    const tinygltf::Image& image = gltf.images[texture.source];

    CHECK_RETURN_IO_ERROR(image.bufferView == -1, IoError::ErrorCode::GeneralError,
        "Failed to import image: image must be external", image.name)

    const std::filesystem::path imagePath = ctx.ScenePath.parent_path() / decodeUri(image.uri);
    ImageImporter importer(ctx.Ctx, {.BakedFormat = imageFormat, .Overwrite = true});
    auto importResult = importer.Import(imagePath);
    CHECK_RETURN_IO_ERROR(importResult.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to import image: {}: {}", image.uri, importResult.error())

    LUX_LOG_INFO("Imported image file: {}, for scene {}", imagePath.string(), ctx.ScenePath.string());

    return importer.GetImportedAssetMetadata().AssetId;
}

IoResult<assetlib::AssetId> importMaterial(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Material& material)
{
    assetlib::MaterialAlphaMode alphaMode = assetlib::MaterialAlphaMode::Opaque;
    if (material.alphaMode == "MASK")
        alphaMode = assetlib::MaterialAlphaMode::Mask;
    else if (material.alphaMode == "BLEND")
        alphaMode = assetlib::MaterialAlphaMode::Translucent;

    auto importedBaseColor = importMaterialImage(ctx, gltf, material.pbrMetallicRoughness.baseColorTexture.index,
        assetlib::ImageFormat::RGBA8_SRGB);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importedBaseColor)

    auto importedEmissive = importMaterialImage(ctx, gltf, material.emissiveTexture.index,
        assetlib::ImageFormat::RGBA8_SRGB);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importedEmissive)

    auto importedNormal = importMaterialImage(ctx, gltf, material.normalTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importedNormal)

    auto importedMetallicRoughness = importMaterialImage(ctx, gltf,
        material.pbrMetallicRoughness.metallicRoughnessTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importedMetallicRoughness)

    auto importedOcclusion = importMaterialImage(ctx, gltf,
        material.occlusionTexture.index,
        assetlib::ImageFormat::RGBA8_UNORM);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importedOcclusion)
    
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
        .BaseColorTexture = *importedBaseColor,
        .EmissiveTexture = *importedEmissive,
        .NormalTexture = *importedNormal,
        .MetallicRoughnessTexture = *importedMetallicRoughness,
        .OcclusionTexture = *importedOcclusion
    };
    
    const std::filesystem::path materialPath = ctx.ScenePath.parent_path() /
        (material.name + std::string(MATERIAL_ASSET_EXTENSION));
    MaterialImporter importer(ctx.Ctx);
    auto exportResult = importer.Export(bakedMaterial, materialPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(exportResult)

    return *exportResult;
}

IoResult<void> processMaterial(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Material& material)
{
    auto convertSampleInfo = [&](i32 textureIndex, i32 uvIndex) -> assetlib::MeshPrimitiveTextureSample
    {
        if (textureIndex < 0)
            return {};

        const auto& texture = gltf.textures[textureIndex];
        const auto& sampler = gltf.samplers[texture.sampler];

        auto filter = assetlib::MeshPrimitiveTextureFilter::Linear;
        switch (sampler.minFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            filter = assetlib::MeshPrimitiveTextureFilter::Nearest;
            break;
        default:
            break;
        }

        return {
            .UvIndex = (u32)uvIndex,
            .Filter = filter
        };
    };

    auto importedMaterial = importMaterial(ctx, gltf, material);
    CHECK_RETURN_IO_ERROR(importedMaterial.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to import material: {} ({})", importedMaterial.error(), material.name)

    ctx.Materials.push_back({
        .MaterialAsset = *importedMaterial,
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
        ctx.SceneAsset.Cameras.push_back({
            .Type = assetlib::SceneAssetCameraType::Perspective,
            .Near = (f32)camera.perspective.znear,
            .Far = (f32)camera.perspective.zfar,
            .Perspective = assetlib::SceneAssetCamera::PerspectiveData{
                .Aspect = (f32)camera.perspective.aspectRatio,
                .FovY = (f32)camera.perspective.yfov
            },
        });
    else
        ctx.SceneAsset.Cameras.push_back({
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

    ctx.SceneAsset.Lights.push_back({
        .Type = lightType,
        .Color = (glm::vec3)*(glm::dvec3*)light.color.data(),
        .Intensity = (f32)light.intensity,
        .Range = light.range > 0 ? (f32)light.range : UNLIMITED_RANGE
    });
}

void processNode(ProcessContext& ctx, tinygltf::Model&, tinygltf::Node& node)
{
    auto getTransform = [](tinygltf::Node& node)
    {
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
        .Skin = (u32)node.skin,
        .Transform = getTransform(node)
    };
    bakedNode.Children.reserve(node.children.size());
    for (i32 child : node.children)
        bakedNode.Children.push_back((u32)child);

    ctx.SceneAsset.Nodes.push_back(std::move(bakedNode));
}

void processSubscene(ProcessContext& ctx, tinygltf::Model&, tinygltf::Scene& subscene)
{
    assetlib::SceneAssetSubscene backedSubscene = {
        .Name = subscene.name,
    };
    backedSubscene.Nodes.reserve(subscene.nodes.size());
    for (i32 node : subscene.nodes)
        backedSubscene.Nodes.push_back(node);

    ctx.SceneAsset.Subscenes.push_back(std::move(backedSubscene));
}

template <typename T>
void copyBufferToVector(std::vector<T>& vec, tinygltf::Model& gltf, const tinygltf::Accessor& accessor)
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

IoResult<std::vector<u32>> readIndices(tinygltf::Model& gltf, const tinygltf::Accessor& indexAccessor)
{
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
    
    return indices;
}

IoResult<std::vector<glm::u16vec4>> readJoints(tinygltf::Model& gltf, const tinygltf::Accessor& jointsAccessor)
{
    std::vector<glm::u16vec4> joints;
    switch (jointsAccessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
            std::vector<glm::u8vec4> rawJoints(jointsAccessor.count);
            copyBufferToVector(rawJoints, gltf, jointsAccessor);
            joints.reserve(rawJoints.size());
            for (auto& joint : rawJoints)
                joints.emplace_back(joint.x, joint.y, joint.z, joint.w);
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            copyBufferToVector(joints, gltf, jointsAccessor);
            break;
        }
    default:
        return std::unexpected(assetlib::io::IoError{
            .Code = IoError::ErrorCode::WrongFormat,
            .Message = std::format("Unexpected joints accessor type: {}", jointsAccessor.componentType)
        });
    }
    
    return joints;
}

IoResult<std::vector<glm::vec4>> readKeyframeOrientations(tinygltf::Model& gltf, 
    const tinygltf::Accessor& keyframesAccessor)
{
    std::vector<glm::vec4> keyframes;
    switch (keyframesAccessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        {
            copyBufferToVector(keyframes, gltf, keyframesAccessor);
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        {
            std::vector<glm::i8vec4> rawKeyframes;
            copyBufferToVector(rawKeyframes, gltf, keyframesAccessor);
            keyframes.reserve(rawKeyframes.size());
            for (auto frame : rawKeyframes) 
                keyframes.push_back(glm::max(glm::vec4(frame) / 127.0f, -1.0f));
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
            std::vector<glm::u8vec4> rawKeyframes;
            copyBufferToVector(rawKeyframes, gltf, keyframesAccessor);
            keyframes.reserve(rawKeyframes.size());
            for (auto frame : rawKeyframes) 
                keyframes.push_back(glm::vec4(frame) / 255.0f);
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        {
            std::vector<glm::i16vec4> rawKeyframes;
            copyBufferToVector(rawKeyframes, gltf, keyframesAccessor);
            keyframes.reserve(rawKeyframes.size());
            for (auto frame : rawKeyframes) 
                keyframes.push_back(glm::max(glm::vec4(frame) / 32767.0f, -1.0f));
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            std::vector<glm::u16vec4> rawKeyframes;
            copyBufferToVector(rawKeyframes, gltf, keyframesAccessor);
            keyframes.reserve(rawKeyframes.size());
            for (auto frame : rawKeyframes) 
                keyframes.push_back(glm::vec4(frame) / 65535.0f);
            break;
        }
    default:
        return std::unexpected(assetlib::io::IoError{
            .Code = IoError::ErrorCode::WrongFormat,
            .Message = std::format("Unexpected keyframe orientations accessor type: {}",
                keyframesAccessor.componentType)
        });
    }
    
    return keyframes;
}

std::string getMeshName(ProcessContext& ctx, tinygltf::Mesh& mesh)
{
    std::string meshName = mesh.name;
    if (meshName.empty())
        meshName = std::format("_unnamed_mesh_{}", ctx.MeshAssets.size());
    
    for (char& c : meshName)
    {
        switch (c)
        {
        case '/':
        case '\\':
        case '*':
        case '?':
        case '\"':
        case '<':
        case '>':
        case '|':
        case ':':
            c = '_';
            break;
        default:
            break;
        }
    }
    
    return meshName;
}

IoResult<void> processMesh(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Mesh& mesh)
{
    for (auto& primitive : mesh.primitives)
    {
        CHECK_RETURN_IO_ERROR(primitive.mode == TINYGLTF_MODE_TRIANGLES, IoError::ErrorCode::GeneralError,
            "Failed to process mesh {}. The mesh mode is not triangles", mesh.name)

        tinygltf::Accessor& indexAccessor = gltf.accessors[primitive.indices];
        auto indices = readIndices(gltf, indexAccessor);
        CHECK_RETURN_IO_ERROR_PROPAGATE(indices)

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> uvs;
        std::vector<glm::u16vec4> joints;
        std::vector<glm::vec4> weights;
        for (auto& attribute : primitive.attributes)
        {
            tinygltf::Accessor& attributeAccessor = gltf.accessors[attribute.second];
            if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_POSITION_NAME)
                copyBufferToVector(positions, gltf, attributeAccessor);
            else if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_NORMAL_NAME)
                copyBufferToVector(normals, gltf, attributeAccessor);
            else if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_TANGENT_NAME)
                copyBufferToVector(tangents, gltf, attributeAccessor);
            else if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_UV0_NAME)
                copyBufferToVector(uvs, gltf, attributeAccessor);
            else if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_JOINTS0_NAME)
            {
                auto jointsRead = readJoints(gltf, attributeAccessor);
                CHECK_RETURN_IO_ERROR_PROPAGATE(jointsRead)
                joints = std::move(*jointsRead);
            }
            else if (attribute.first == assetlib::MeshPrimitive::ATTRIBUTE_WEIGHTS0_NAME)
                copyBufferToVector(weights, gltf, attributeAccessor);
        }
        CHECK_RETURN_IO_ERROR(!positions.empty(), IoError::ErrorCode::GeneralError,
            "Failed to process mesh {}. The mesh has no positions data", mesh.name)

        const bool hasNormals = !normals.empty();
        const bool hasTangents = !tangents.empty();
        const bool hasUVs = !uvs.empty();

        if (!hasNormals)
            generateTriangleNormals(normals, positions, *indices);
        if (!hasTangents && hasUVs)
            generateTriangleTangents(tangents, positions, normals, uvs, *indices);
        if (!hasTangents)
            tangents.resize(positions.size(), glm::vec4{0.0f, 0.0f, 1.0f, 1.0f});
        if (!hasUVs)
            uvs.resize(positions.size(), glm::vec2{0.0});

        utils::Attributes attributes{
            .Positions = &positions,
            .Normals = &normals,
            .Tangents = &tangents,
            .UVs = &uvs,
            .Joints = &joints,
            .Weights = &weights
        };
        utils::remapMesh(attributes, *indices);
        auto&& [meshlets, meshletIndices] = utils::createMeshlets(attributes, *indices);
        auto&& [sphere, box] = utils::meshBoundingVolumes(meshlets);
        
        std::vector<assetlib::MeshPrimitive::Attribute> meshAttributes;
        auto addAttribute = [&ctx, &meshAttributes](auto& source, auto& destination, std::string_view name) {
            u32 accessorIndex = (u32)ctx.GeometryBufferAsset.Header.Accessors.size();
            ctx.GeometryBufferAsset.Header.Accessors.push_back(ctx.CreateAccessor(source, destination));
            meshAttributes.push_back(assetlib::MeshPrimitive::Attribute{
                .Name = std::string(name),
                .Accessor = accessorIndex
            });
        };
        
        addAttribute(positions, ctx.Positions, assetlib::MeshPrimitive::ATTRIBUTE_POSITION_NAME);
        addAttribute(normals, ctx.Normals, assetlib::MeshPrimitive::ATTRIBUTE_NORMAL_NAME);
        addAttribute(tangents, ctx.Tangents, assetlib::MeshPrimitive::ATTRIBUTE_TANGENT_NAME);
        addAttribute(uvs, ctx.UVs, assetlib::MeshPrimitive::ATTRIBUTE_UV0_NAME);
        if (!joints.empty())
        {
            ASSERT(joints.size() == weights.size())
            addAttribute(joints, ctx.Joints, assetlib::MeshPrimitive::ATTRIBUTE_JOINTS0_NAME);
            addAttribute(weights, ctx.Weights, assetlib::MeshPrimitive::ATTRIBUTE_WEIGHTS0_NAME);
        }
        addAttribute(meshlets, ctx.Meshlets, assetlib::MeshPrimitive::ATTRIBUTE_MESHLET_NAME);

        const u32 indicesAccessorIndex = (u32)ctx.GeometryBufferAsset.Header.Accessors.size();
        ctx.GeometryBufferAsset.Header.Accessors.push_back(ctx.CreateAccessor(meshletIndices, ctx.Indices));
        assetlib::MeshPrimitive bakedPrimitive = {
            .Attributes = std::move(meshAttributes),
            .Material = primitive.material == -1 ? assetlib::MeshPrimitiveMaterial{} : ctx.Materials[primitive.material],
            .IndicesAccessor = indicesAccessorIndex,
            .BoundingSphere = sphere,
            .BoundingBox = box,
        };
        
        ctx.MeshAssets.push_back({.Asset = {.Primitives = {bakedPrimitive}}, .Name = getMeshName(ctx, mesh)});
    }

    return {};
}

void processMeshMaterialsOnly(ProcessContext& ctx, tinygltf::Mesh& mesh)
{
    for (auto& primitive : mesh.primitives)
    {
        assetlib::MeshPrimitive bakedPrimitive = {
            .Material = primitive.material == -1 ? assetlib::MeshPrimitiveMaterial{} : ctx.Materials[primitive.material]
        };
        
        std::string meshName = mesh.name;
        if (meshName.empty())
            meshName = std::format("_unnamed_mesh_{}", ctx.MeshAssets.size());
        ctx.MeshAssets.push_back({.Asset = {.Primitives = {bakedPrimitive}}, .Name = getMeshName(ctx, mesh)});
    }
}

IoResult<void> processSkin(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Skin& skin)
{
    tinygltf::Accessor& attributeAccessor = gltf.accessors[skin.inverseBindMatrices];
    
    std::vector<glm::mat4> matrices(attributeAccessor.count);
    copyBufferToVector(matrices, gltf, attributeAccessor);

    const u32 accessorIndex = (u32)ctx.GeometryBufferAsset.Header.Accessors.size();
    ctx.GeometryBufferAsset.Header.Accessors.push_back(ctx.CreateAccessor(matrices, ctx.InverseBindMatrices));
    
    std::vector<u32> joints;
    joints.assign_range(skin.joints);
    ctx.SceneAsset.Skins.push_back({
        .InverseBindMatrixAccessor = accessorIndex,
        .JointNodes = std::move(joints)
    });
    
    return {};
}

IoResult<assetlib::SceneAssetAnimationChannelType> getAnimationChannelType(const tinygltf::AnimationChannel& channel)
{
    if (channel.target_path == "translation")
        return assetlib::SceneAssetAnimationChannelType::Translation;
    if (channel.target_path == "rotation")
        return assetlib::SceneAssetAnimationChannelType::Orientation;
    if (channel.target_path == "scale")
        return assetlib::SceneAssetAnimationChannelType::Scale;

    CHECK_RETURN_IO_ERROR(false, IoError::ErrorCode::GeneralError, "Unsupported animation channel type: {}",
        channel.target_path)
}

IoResult<assetlib::SceneAssetAnimationSamplerType> getAnimationSamplerType(const tinygltf::AnimationSampler& sampler)
{
    if (sampler.interpolation == "LINEAR")
        return assetlib::SceneAssetAnimationSamplerType::Linear;
    if (sampler.interpolation == "STEP")
        return assetlib::SceneAssetAnimationSamplerType::Step;
    if (sampler.interpolation == "CUBICSPLINE")
        return assetlib::SceneAssetAnimationSamplerType::CubicSpline;

    CHECK_RETURN_IO_ERROR(false, IoError::ErrorCode::GeneralError, "Unsupported animation sampler type: {}",
        sampler.interpolation)
}

IoResult<void> processAnimation(ProcessContext& ctx, tinygltf::Model& gltf, tinygltf::Animation& animation)
{
    std::vector<assetlib::SceneAssetAnimationChannel> channels;
    channels.reserve(animation.channels.size());
    
    for (auto& channel : animation.channels)
    {
        if (channel.target_node == -1)
            continue;

        auto channelType = getAnimationChannelType(channel);
        CHECK_RETURN_IO_ERROR_PROPAGATE(channelType)
        
        const tinygltf::AnimationSampler sampler = animation.samplers[channel.sampler];
        auto samplerType = getAnimationSamplerType(sampler);
        CHECK_RETURN_IO_ERROR_PROPAGATE(samplerType)
        
        tinygltf::Accessor& timestampAccessor = gltf.accessors[sampler.input];
        std::vector<f32> timestamps;
        copyBufferToVector(timestamps, gltf, timestampAccessor);
        u32 timestampAccessorIndex = (u32)ctx.GeometryBufferAsset.Header.Accessors.size();
        ctx.GeometryBufferAsset.Header.Accessors.push_back(ctx.CreateAccessor(timestamps, ctx.AnimationTimestamps));
        
        tinygltf::Accessor& keyframeAccessor = gltf.accessors[sampler.output];
        u32 keyframeAccessorIndex = (u32)ctx.GeometryBufferAsset.Header.Accessors.size();
        switch (*channelType) 
        {
        case assetlib::SceneAssetAnimationChannelType::Translation:
            {
                std::vector<glm::vec3> positions;
                copyBufferToVector(positions, gltf, keyframeAccessor);
                ctx.GeometryBufferAsset.Header.Accessors.push_back(
                    ctx.CreateAccessor(positions, ctx.AnimationPositionKeyframes));
                break;
            }
        case assetlib::SceneAssetAnimationChannelType::Orientation:
            {
                auto readOrientations = readKeyframeOrientations(gltf, keyframeAccessor);
                CHECK_RETURN_IO_ERROR_PROPAGATE(readOrientations)
                std::vector<glm::vec4> orientations = std::move(*readOrientations);
                ctx.GeometryBufferAsset.Header.Accessors.push_back(
                    ctx.CreateAccessor(orientations, ctx.AnimationOrientationKeyframes));
            }
            break;
        case assetlib::SceneAssetAnimationChannelType::Scale:
            {
                std::vector<glm::vec3> scales;
                copyBufferToVector(scales, gltf, keyframeAccessor);
                ctx.GeometryBufferAsset.Header.Accessors.push_back(
                    ctx.CreateAccessor(scales, ctx.AnimationScaleKeyframes));
                break;
            }
        }
        
        channels.push_back({
            .Type = *channelType,
            .SamplerType = *samplerType,
            .TargetNode = (u32)channel.target_node,
            .TimestampsAccessor = timestampAccessorIndex,
            .KeyframesAccessor = keyframeAccessorIndex
        });
    }
    
    ctx.SceneAsset.Animations.push_back({
        .Name = animation.name,
        .Channels = std::move(channels) 
    });
    
    return {};
}

IoResult<void> createGeometryBufferAssets(ProcessContext& ctx)
{
    GeometryBufferImporter importer(ctx.Ctx, {
        .IsSubAsset = true,
        .SourceHash = ctx.BufferHash,
        .SourceUri = ctx.BufferUri
    });
    
    auto exportResult = importer.Export(ctx.GeometryBufferAsset, ctx.ScenePath.parent_path() / ctx.BufferUri);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(exportResult)
    
    ctx.GeometryBufferAssetId = *exportResult;
    
    return {};
}

std::filesystem::path getMeshAssetPath(ProcessContext& ctx, ProcessContext::MeshInfo& mesh)
{
    return ctx.ScenePath.parent_path() / mesh.Name;
}

IoResult<void> createMeshAssets(ProcessContext& ctx)
{
    MeshImporter importer(ctx.Ctx);
    
    for (auto& mesh : ctx.MeshAssets)
    {
        auto& asset = mesh.Asset;
        asset.GeometryBuffer = ctx.GeometryBufferAssetId;
        auto exportResult = importer.Export(asset, getMeshAssetPath(ctx, mesh));
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(exportResult)
        mesh.AssetId = *exportResult;
    }
    
    ctx.SceneAsset.Meshes.reserve(ctx.MeshAssets.size());
    for (auto& mesh : ctx.MeshAssets)
        ctx.SceneAsset.Meshes.push_back(mesh.AssetId);
    
    return {};
}

IoResult<void> updateExistingMeshesMaterials(ProcessContext& ctx)
{
    MeshImporter importer(ctx.Ctx);
    
    for (auto& mesh : ctx.MeshAssets)
    {
        auto importResult = importer.Import(getMeshAssetPath(ctx, mesh));
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importResult)
        auto existingMesh = importer.GetImportedMesh();
        
        CHECK_RETURN_IMPORT_ERROR(existingMesh.Asset.Primitives.size() == mesh.Asset.Primitives.size(), 
            IoError::ErrorCode::GeneralError, "Meshes primitive count changed")
        
        for (u32 primitiveIndex = 0; primitiveIndex < mesh.Asset.Primitives.size(); primitiveIndex++) 
            existingMesh.Asset.Primitives[primitiveIndex].Material = mesh.Asset.Primitives[primitiveIndex].Material;
        
        auto exportResult = importer.Export(existingMesh.Asset, getMeshAssetPath(ctx, mesh));
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(exportResult)
        mesh.AssetId = *exportResult;
    }
    
    ctx.SceneAsset.Meshes.reserve(ctx.MeshAssets.size());
    for (auto& mesh : ctx.MeshAssets)
        ctx.SceneAsset.Meshes.push_back(mesh.AssetId);
    
    return {};
}

IoResult<void> copyExistingGeometry(ProcessContext& ctx, tinygltf::Model& gltf)
{
    SceneImporter importer(ctx.Ctx);
    auto imported = importer.Import(ctx.ScenePath, ImportFlags::Header | ImportFlags::Outdated);
    CHECK_RETURN_IO_ERROR_PROPAGATE(imported)
    
    const auto& asset = importer.GetImportedScene().Asset;
    
    for (auto& mesh : gltf.meshes)
        processMeshMaterialsOnly(ctx, mesh);
    CHECK_RETURN_IO_ERROR_PROPAGATE(updateExistingMeshesMaterials(ctx))
    
    ctx.SceneAsset.Skins.reserve(asset.Skins.size());
    for (auto& skin : asset.Skins)
        ctx.SceneAsset.Skins.push_back(skin);
    
    ctx.SceneAsset.Animations.reserve(asset.Animations.size());
    for (auto& animation : asset.Animations)
        ctx.SceneAsset.Animations.push_back(animation);
    
    return {};
}

IoResult<void> processGeometry(ProcessContext& ctx, tinygltf::Model& gltf)
{
    CHECK_RETURN_IO_ERROR(gltf.buffers.size() < 2, IoError::ErrorCode::GeneralError,
        "Failed to bake scene: only one geometry buffer is supported")
    
    GeometryBufferImporter importer(ctx.Ctx, {
        .IsSubAsset = true,
        .SourceHash = ctx.BufferHash,
        .SourceUri = ctx.BufferUri
    });
    const bool needToRebakeGeometry = importer.NeedsBaking(ctx.ScenePath.parent_path() / ctx.BufferUri);
    if (!needToRebakeGeometry && copyExistingGeometry(ctx, gltf).has_value())
        return {};
 
    ctx.MeshAssets.reserve(gltf.meshes.size());
    for (auto& mesh : gltf.meshes)
    {
        auto meshProcessResult = processMesh(ctx, gltf, mesh);
        CHECK_RETURN_IO_ERROR_PROPAGATE(meshProcessResult)
    }
    
    ctx.SceneAsset.Skins.reserve(gltf.skins.size());
    for (auto& skin : gltf.skins)
    {
        auto skinProcessResult = processSkin(ctx, gltf, skin);
        CHECK_RETURN_IO_ERROR_PROPAGATE(skinProcessResult)
    }
    
    ctx.SceneAsset.Animations.reserve(gltf.animations.size());
    for (auto& animation : gltf.animations)
    {
        auto animationProcessResult = processAnimation(ctx, gltf, animation);
        CHECK_RETURN_IO_ERROR_PROPAGATE(animationProcessResult)
    }
    ctx.FinalizeGeometry();
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(createGeometryBufferAssets(ctx))
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(createMeshAssets(ctx))
    for (auto& skin : ctx.SceneAsset.Skins)
        skin.GeometryBuffer = ctx.GeometryBufferAssetId;
    for (auto& animation : ctx.SceneAsset.Animations)
        animation.GeometryBuffer = ctx.GeometryBufferAssetId;
    
    return {};
}
}

IoResult<assetlib::SceneAsset> SceneBaker::Bake(const assetlib::SceneMeta& meta)
{
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;

    std::string errors;
    std::string warnings;
    const std::filesystem::path path = meta.Metadata.Io.OriginalFile;
    const bool success = loader.LoadASCIIFromFile(&gltf, &errors, &warnings, path.string());

    CHECK_RETURN_IO_ERROR(errors.empty() && success, IoError::ErrorCode::GeneralError,
        "Failed to bake scene: {} ({})", errors, path.string())

    /* process just one scene for now */
    const u32 sceneIndex = gltf.defaultScene > 0 ? (u32)gltf.defaultScene : 0;
    auto& scene = gltf.scenes[sceneIndex];
    CHECK_RETURN_IO_ERROR(!scene.nodes.empty(), IoError::ErrorCode::GeneralError,
        "Failed to bake scene: no nodes in default scene")
    CHECK_RETURN_IO_ERROR(gltf.buffers.size() < 2, IoError::ErrorCode::GeneralError,
        "Failed to bake scene: only one geometry buffer is supported")
    
    ProcessContext processCtx = {
        .ScenePath = path,
        .BufferUri = gltf.buffers.front().uri,
        .BufferHash = SceneImporter::CalculateGeometryBufferHash(path, gltf.buffers.front().uri).value_or(0),
        .Ctx = m_Ctx,
    };

    for (auto& material : gltf.materials)
    {
        auto materialProcessResult = processMaterial(processCtx, gltf, material);
        CHECK_RETURN_IO_ERROR(materialProcessResult.has_value(), IoError::ErrorCode::GeneralError,
            "Failed to bake scene: {} ({})", materialProcessResult.error(), path.string())
    }

    for (auto& camera : gltf.cameras)
        processCamera(processCtx, gltf, camera);

    for (auto& light : gltf.lights)
        processLight(processCtx, gltf, light);

    for (auto& node : gltf.nodes)
        processNode(processCtx, gltf, node);

    processSubscene(processCtx, gltf, scene);
    processCtx.SceneAsset.DefaultSubscene = sceneIndex;

    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(processGeometry(processCtx, gltf))
    
    return assetlib::SceneAsset{std::move(processCtx.SceneAsset)};
}
}
