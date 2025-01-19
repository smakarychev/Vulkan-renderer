#include "ModelAsset.h"

#include <nlohmann/json.hpp>

#include "lz4.h"
#include "utils.h"
#include "core.h"

namespace
{
    assetLib::VertexFormat parseFormatString(std::string_view format)
    {
        if (format == "P3N3T3UV2")
            return assetLib::VertexFormat::P3N3T3UV2;
        return assetLib::VertexFormat::Unknown;
    }

    std::string vertexFormatToString(assetLib::VertexFormat format)
    {
        switch (format)
        {
        case assetLib::VertexFormat::P3N3T3UV2:
            return "P3N3T3UV2";
        default:
            LOG("Unsupported vertex format");
            break;
        }
        std::unreachable();
    }

    assetLib::ModelInfo::MaterialAspect parseMaterialAspectString(std::string_view materialString)
    {
        if (materialString == "albedo")
            return assetLib::ModelInfo::MaterialAspect::Albedo;
        if (materialString == "normal")
            return assetLib::ModelInfo::MaterialAspect::Normal;
        if (materialString == "metallic_roughness")
            return assetLib::ModelInfo::MaterialAspect::MetallicRoughness;
        if (materialString == "ambient_occlusion")
            return assetLib::ModelInfo::MaterialAspect::AmbientOcclusion;
        if (materialString == "emissive")
            return assetLib::ModelInfo::MaterialAspect::Emissive;
        LOG("Unrecognized material string");
        std::unreachable();
    }

    std::string materialAspectToString(assetLib::ModelInfo::MaterialAspect aspect)
    {
        switch (aspect)
        {
        case assetLib::ModelInfo::MaterialAspect::Albedo:
            return "albedo";
        case assetLib::ModelInfo::MaterialAspect::Normal:
            return "normal";
        case assetLib::ModelInfo::MaterialAspect::MetallicRoughness:
            return "metallic_roughness";
        case assetLib::ModelInfo::MaterialAspect::AmbientOcclusion:
            return "ambient_occlusion";
        case assetLib::ModelInfo::MaterialAspect::Emissive:
            return "emissive";
        default:
            LOG("Unsupported material aspect type");
            break;
        }
        std::unreachable();
    }

    assetLib::ModelInfo::MaterialType parseMaterialTypeString(std::string_view materialString)
    {
        if (materialString == "opaque")
            return assetLib::ModelInfo::MaterialType::Opaque;
        if (materialString == "translucent")
            return assetLib::ModelInfo::MaterialType::Translucent;
        LOG("Unrecognized material string");
        std::unreachable();
    }

    std::string materialTypeToString(assetLib::ModelInfo::MaterialType type)
    {
        switch (type)
        {
        case assetLib::ModelInfo::MaterialType::Opaque:
            return "opaque";
        case assetLib::ModelInfo::MaterialType::Translucent:
            return "translucent";
        default:
            LOG("Unsupported material type");
            break;
        }
        std::unreachable();
    }
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


namespace assetLib
{
    std::array<const void*, (u32)VertexElement::MaxVal> VertexGroup::Elements()
    {
        return {Positions.data(), Normals.data(), Tangents.data(), UVs.data()};
    }

    std::array<u64, (u32)VertexElement::MaxVal> VertexGroup::ElementsSizesBytes()
    {
        return {
            Positions.size() * sizeof(glm::vec3),
            Normals.size() * sizeof(glm::vec3),
            Tangents.size() * sizeof(glm::vec3),
            UVs.size() * sizeof(glm::vec2)};        
    }

    std::vector<u64> ModelInfo::VertexElementsSizeBytes() const
    {
        if (MeshInfos.empty())
            return {};

        std::vector<u64> sizeBytes(MeshInfos.front().VertexElementsSizeBytes.size());
        for (auto& mesh : MeshInfos)
            for (u32 subSize = 0; subSize < sizeBytes.size(); subSize++)
                sizeBytes[subSize] += mesh.VertexElementsSizeBytes[subSize];

        return sizeBytes;
    }

    u64 ModelInfo::IndicesSizeBytes() const
    {
        return std::accumulate(MeshInfos.begin(), MeshInfos.end(),
            0llu, [](u64 size, const auto& mesh){ return size + mesh.IndicesSizeBytes; });
    }

    u64 ModelInfo::MeshletsSizeBytes() const
    {
        return std::accumulate(MeshInfos.begin(), MeshInfos.end(),
            0llu, [](u64 size, const auto& mesh){ return size + mesh.MeshletsSizeBytes; });
    }

    ModelInfo readModelInfo(const assetLib::File& file)
    {
        ModelInfo info = {};

        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        std::string vertexFormatString = metadata["format"];
        info.VertexFormat = parseFormatString(vertexFormatString);
        
        const nlohmann::json& meshes = metadata["meshes_info"];
        info.MeshInfos.reserve(meshes.size());
        for (auto& mesh : meshes)
        {
            ModelInfo::MeshInfo meshInfo = {};

            meshInfo.Name = mesh["name"];

            const nlohmann::json& vertexElementSizeBytes = mesh["vertex_elements_size_bytes"];
            for (u32 vertexElementIndex = 0; vertexElementIndex < vertexElementSizeBytes.size(); vertexElementIndex++)
                meshInfo.VertexElementsSizeBytes[vertexElementIndex] = vertexElementSizeBytes[vertexElementIndex];

            u64 indicesSizeBytes = mesh["indices_size_bytes"];
            meshInfo.IndicesSizeBytes = indicesSizeBytes;

            u64 meshletsSizeBytes = mesh["meshlets_size_bytes"];
            meshInfo.MeshletsSizeBytes = meshletsSizeBytes;

            std::string materialTypeString = mesh["material_type"];
            ModelInfo::MaterialType materialType = parseMaterialTypeString(materialTypeString);
            meshInfo.MaterialType = materialType;

            const nlohmann::json& materialPBR = mesh["pbr_info"];
            ModelInfo::MaterialPropertiesPBR propertiesPBR = {};
            propertiesPBR.Albedo = materialPBR["color"];
            propertiesPBR.Metallic = materialPBR["metallic"];
            propertiesPBR.Roughness = materialPBR["roughness"];
            meshInfo.MaterialPropertiesPBR = propertiesPBR;

            const nlohmann::json& materials = mesh["materials"];
            for (auto& material : materials)
            {
                std::string materialAspectString = material["aspect"];
                ModelInfo::MaterialAspect materialAspect = parseMaterialAspectString(materialAspectString);
                ModelInfo::MaterialInfo materialInfo = {};

                const nlohmann::json& textures = material["textures"];
                materialInfo.Textures.reserve(textures.size());
                for (auto& texture : textures)
                    materialInfo.Textures.push_back(texture);

                meshInfo.Materials[(u32)materialAspect] = materialInfo;
            }

            const nlohmann::json& boundingSphere = mesh["bounding_sphere"];
            meshInfo.BoundingSphere.Center = boundingSphere["center"];
            meshInfo.BoundingSphere.Radius = boundingSphere["radius"];

            const nlohmann::json& boundingBox = mesh["bounding_box"];
            meshInfo.BoundingBox.Min = boundingBox["min"];
            meshInfo.BoundingBox.Max = boundingBox["max"];
            
            info.MeshInfos.push_back(meshInfo);
        }

        unpackAssetInfo(info, &metadata);

        return info;
    }

    assetLib::File packModel(const ModelInfo& info, const std::vector<const void*>& vertices, void* indices, void* meshlets)
    {
        nlohmann::json metadata;

        metadata["format"] = vertexFormatToString(VertexFormat::P3N3T3UV2);

        metadata["meshes_info"] = nlohmann::json::array();
        for (auto& mesh : info.MeshInfos)
        {
            nlohmann::json meshJson;
            meshJson["name"] = mesh.Name;
            meshJson["vertex_elements_size_bytes"] = nlohmann::json::array();
            for (auto elementSizeBytes : mesh.VertexElementsSizeBytes)
                meshJson["vertex_elements_size_bytes"].push_back(elementSizeBytes);

            meshJson["indices_size_bytes"] = mesh.IndicesSizeBytes;

            meshJson["meshlets_size_bytes"] = mesh.MeshletsSizeBytes;

            meshJson["material_type"] = materialTypeToString(mesh.MaterialType);

            nlohmann::json materialPBRJson;
            materialPBRJson["color"] = mesh.MaterialPropertiesPBR.Albedo;
            materialPBRJson["metallic"] = mesh.MaterialPropertiesPBR.Metallic;
            materialPBRJson["roughness"] = mesh.MaterialPropertiesPBR.Roughness;
            meshJson["pbr_info"] = materialPBRJson;
            
            meshJson["materials"] = nlohmann::json::array();
            for (u32 i = 0; i < mesh.Materials.size(); i++)
            {
                auto& material = mesh.Materials[i];
                nlohmann::json materialJson;
                materialJson["aspect"] = materialAspectToString((ModelInfo::MaterialAspect)i);

                materialJson["textures"] = nlohmann::json::array();
                for (auto& texture : material.Textures)
                    materialJson["textures"].push_back(texture);

                meshJson["materials"].push_back(materialJson);
            }

            nlohmann::json boundingSphere;
            boundingSphere["center"] = mesh.BoundingSphere.Center;
            boundingSphere["radius"] = mesh.BoundingSphere.Radius;
            meshJson["bounding_sphere"] = boundingSphere;

            nlohmann::json boundingBox;
            boundingBox["min"] = mesh.BoundingBox.Min;
            boundingBox["max"] = mesh.BoundingBox.Max;
            meshJson["bounding_box"] = boundingBox;
            
            metadata["meshes_info"].push_back(meshJson);
        }
        
        packAssetInfo(info, &metadata);
        
        assetLib::File assetFile = {};

        std::vector<const void*> meshData = vertices;
        meshData.push_back(indices);
        meshData.push_back(meshlets);

        std::vector<u64> sizesBytes = info.VertexElementsSizeBytes();
        sizesBytes.push_back(info.IndicesSizeBytes());
        sizesBytes.push_back(info.MeshletsSizeBytes());
        
        u64 blobSizeBytes = Utils::compressToBlob(assetFile.Blob, meshData, sizesBytes);
        metadata["asset"]["blob_size_bytes"] = blobSizeBytes;
        metadata["asset"]["type"] = assetTypeToString(AssetType::Model);

        assetFile.JSON = metadata.dump(JSON_INDENT);

        return assetFile;        
    }

    void unpackModel(ModelInfo& info, const u8* source, u64 sourceSizeBytes, const std::vector<u8*>& vertices, u8* indices, u8* meshlets)
    {
        std::vector<u64> vertexElementsSizeBytes = info.VertexElementsSizeBytes();
        u64 vertexElementsSizeBytesTotal = std::accumulate(vertexElementsSizeBytes.begin(), vertexElementsSizeBytes.end(), 0llu);
        u64 indicesSizeTotal = info.IndicesSizeBytes();
        u64 meshletsSizeTotal = info.MeshletsSizeBytes();

        u64 totalsize = vertexElementsSizeBytesTotal + indicesSizeTotal + meshletsSizeTotal;
        
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != totalsize)
        {
            std::vector<u8> combined(totalsize);
            LZ4_decompress_safe((const char*)source, (char*)combined.data(), (i32)sourceSizeBytes, (i32)totalsize);
            u64 offset = 0;
            for (u32 elementIndex = 0; elementIndex < vertexElementsSizeBytes.size(); elementIndex++)
            {
                memcpy(vertices[elementIndex], combined.data() + offset, vertexElementsSizeBytes[elementIndex]);
                offset += vertexElementsSizeBytes[elementIndex];
            }
            memcpy(indices, combined.data() + vertexElementsSizeBytesTotal, indicesSizeTotal);
            memcpy(meshlets, combined.data() + vertexElementsSizeBytesTotal + indicesSizeTotal, meshletsSizeTotal);
        }
        else
        {
            u64 offset = 0;
            for (u32 elementIndex = 0; elementIndex < vertexElementsSizeBytes.size(); elementIndex++)
            {
                memcpy(vertices[elementIndex], source + offset, vertexElementsSizeBytes[elementIndex]);
                offset += vertexElementsSizeBytes[elementIndex];
            }
            memcpy(indices, source + vertexElementsSizeBytesTotal, indicesSizeTotal);
            memcpy(indices, source + vertexElementsSizeBytesTotal + indicesSizeTotal, meshletsSizeTotal);
        }
    }
}
