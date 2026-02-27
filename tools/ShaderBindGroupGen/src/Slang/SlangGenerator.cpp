#include "SlangGenerator.h"

#include "GeneratorUtils.h"
#include "SlangUniformTypeGenerator.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"
#include "v2/Shaders/ShaderAsset.h"
#include "v2/Shaders/ShaderLoadInfo.h"

#include <ranges>
#include <unordered_set>

namespace
{
struct BindingsInfo
{
public:
    struct Signature
    {
        std::string Name;
        u32 Count{1};

        bool operator==(const Signature& other) const
        {
            return Name == other.Name && Count == other.Count;
        }
    };

    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = Hash::string(signature.Name);
            Hash::combine(hash, std::hash<u32>{}(signature.Count));

            return hash;
        }
    };

    struct AccessVariant
    {
        std::vector<u32> Variants;
        lux::assetlib::ShaderStage ShaderStages{};
        u32 Binding{0};
        lux::assetlib::ShaderBindingType Type{lux::assetlib::ShaderBindingType::None};
        lux::assetlib::ShaderBindingAccess Access{lux::assetlib::ShaderBindingAccess::Read};
        lux::assetlib::ShaderBindingAttributes Attributes{lux::assetlib::ShaderBindingAttributes::None};
    };

    std::unordered_map<Signature, std::vector<AccessVariant>, SignatureHasher> Bindings;

public:
    void AddAccessVariant(const Signature& signature, const AccessVariant& accessVariant)
    {
        ASSERT(accessVariant.Variants.size() == 1)

        if (!Bindings.contains(signature))
        {
            Bindings[signature] = {accessVariant};
            return;
        }

        const auto it = std::ranges::find_if(Bindings[signature], [accessVariant](auto& access)
        {
            return
                access.ShaderStages == accessVariant.ShaderStages &&
                access.Binding == accessVariant.Binding &&
                access.Type == accessVariant.Type &&
                access.Access == accessVariant.Access &&
                access.Attributes == accessVariant.Attributes;
        });
        if (it == Bindings[signature].end())
            Bindings[signature].push_back(accessVariant);
        else
            it->Variants.push_back(accessVariant.Variants.front());
    }

    bool HasSamplers(const Signature& signature) const
    {
        return std::ranges::any_of(Bindings.at(signature), [](auto& access)
        {
            return access.Type == lux::assetlib::ShaderBindingType::Sampler;
        });
    }

    bool OnlySamplers(const Signature& signature) const
    {
        return std::ranges::all_of(Bindings.at(signature), [](auto& access)
        {
            return access.Type == lux::assetlib::ShaderBindingType::Sampler;
        });
    }
};

bool bindingAccessIsDivergent(const std::vector<BindingsInfo::AccessVariant>& accesses,
    const std::vector<std::string>& variants)
{
    // access is divergent if binding is used differently across variants
    // (e.g. read-only in one variant and read-write in other),
    // or if it is not present in some variants (e.g. binding declaration is hidden behind #ifdef)
    return accesses.size() != 1 || accesses.front().Variants.size() < variants.size();
}

struct UniformBindingsInfo
{
    struct Signature
    {
        std::string Type;
        std::string Parameter;
        BindingsInfo::Signature BindingSignature;

        bool operator==(const Signature& other) const
        {
            return Type == other.Type && Parameter == other.Parameter && BindingSignature == other.BindingSignature;
        }
    };

    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = Hash::string(signature.Type);
            Hash::combine(hash, Hash::string(signature.Parameter));
            Hash::combine(hash, BindingsInfo::SignatureHasher{}(signature.BindingSignature));

            return hash;
        }
    };

    std::unordered_map<Signature, std::vector<u32>, SignatureHasher> Bindings;

public:
    void Add(const Signature& signature, u32 variant)
    {
        Bindings[signature].push_back(variant);
    }
};

struct BindingSetsInfo
{
    struct Signature
    {
        bool HasImmutableSamplers{false};
        u32 Set{};

        bool operator==(const Signature& other) const
        {
            return HasImmutableSamplers == other.HasImmutableSamplers && Set == other.Set;
        }
    };

    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = std::hash<bool>{}(signature.HasImmutableSamplers);
            Hash::combine(hash, std::hash<u32>{}(signature.Set));

            return hash;
        }
    };

    std::unordered_map<Signature, std::vector<u32>, SignatureHasher> Sets;

public:
    void Add(const Signature& signature, u32 variant)
    {
        Sets[signature].push_back(variant);
    }
};

std::string_view accessToString(lux::assetlib::ShaderBindingAccess access)
{
    switch (access)
    {
    case lux::assetlib::ShaderBindingAccess::Read: return "Read";
    case lux::assetlib::ShaderBindingAccess::Write: return "Write";
    case lux::assetlib::ShaderBindingAccess::ReadWrite: return "ReadWrite";
    default:
        ASSERT(false)
        return "Read";
    }
}

std::string_view bindingTypeToString(lux::assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case lux::assetlib::ShaderBindingType::Image:
    case lux::assetlib::ShaderBindingType::ImageStorage:
        return "Image";
    case lux::assetlib::ShaderBindingType::TexelUniform:
    case lux::assetlib::ShaderBindingType::TexelStorage:
    case lux::assetlib::ShaderBindingType::UniformBuffer:
    case lux::assetlib::ShaderBindingType::UniformTexelBuffer:
    case lux::assetlib::ShaderBindingType::StorageBuffer:
    case lux::assetlib::ShaderBindingType::StorageTexelBuffer:
        return "Buffer";
    default:
        ASSERT(false)
        return "Buffer";
    }
}

std::string shaderStagesToGraphUsage(lux::assetlib::ShaderStage stage)
{
    std::string stages = {};
    if (enumHasAny(stage, lux::assetlib::ShaderStage::Vertex))
        stages += "Vertex";
    if (enumHasAny(stage, lux::assetlib::ShaderStage::Pixel))
        stages += stages.empty() ? "Pixel" : " | Pixel";
    if (enumHasAny(stage, lux::assetlib::ShaderStage::Compute))
        stages += stages.empty() ? "Compute" : " | Compute";

    return stages;
}

std::string bindingTypeToGraphUsage(lux::assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case lux::assetlib::ShaderBindingType::Image:
    case lux::assetlib::ShaderBindingType::TexelUniform:
        return "Sampled";
    case lux::assetlib::ShaderBindingType::ImageStorage:
    case lux::assetlib::ShaderBindingType::TexelStorage:
        return "Storage";
    case lux::assetlib::ShaderBindingType::UniformBuffer:
        return "Uniform";
    case lux::assetlib::ShaderBindingType::UniformTexelBuffer:
    case lux::assetlib::ShaderBindingType::StorageBuffer:
    case lux::assetlib::ShaderBindingType::StorageTexelBuffer:
        return "Storage";
    default:
        ASSERT(false)
        return "";
    }
}

bool isBuffer(lux::assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case lux::assetlib::ShaderBindingType::TexelUniform:
    case lux::assetlib::ShaderBindingType::TexelStorage:
    case lux::assetlib::ShaderBindingType::UniformBuffer:
    case lux::assetlib::ShaderBindingType::UniformTexelBuffer:
    case lux::assetlib::ShaderBindingType::StorageBuffer:
    case lux::assetlib::ShaderBindingType::StorageTexelBuffer:
        return true;
    default:
        return false;
    }
}

std::string_view bindingTypeToDescriptorTypeString(lux::assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case lux::assetlib::ShaderBindingType::Sampler: return "DescriptorType::Sampler";
    case lux::assetlib::ShaderBindingType::Image: return "DescriptorType::Image";
    case lux::assetlib::ShaderBindingType::ImageStorage: return "DescriptorType::ImageStorage";
    case lux::assetlib::ShaderBindingType::TexelUniform: return "DescriptorType::TexelUniform";
    case lux::assetlib::ShaderBindingType::TexelStorage: return "DescriptorType::TexelStorage";
    case lux::assetlib::ShaderBindingType::UniformBuffer: return "DescriptorType::UniformBuffer";
    case lux::assetlib::ShaderBindingType::StorageBuffer: return "DescriptorType::StorageBuffer";
    case lux::assetlib::ShaderBindingType::UniformBufferDynamic: return "DescriptorType::UniformBufferDynamic";
    case lux::assetlib::ShaderBindingType::StorageBufferDynamic: return "DescriptorType::StorageBufferDynamic";
    case lux::assetlib::ShaderBindingType::Input: return "DescriptorType::Input";
    default:
        ASSERT(false)
        return "";
    }
}

std::string_view formatFromImageFormat(lux::assetlib::ImageFormat format)
{
    switch (format)
    {
    case lux::assetlib::ImageFormat::Undefined: return "Format::Undefined";
    case lux::assetlib::ImageFormat::RG4_UNORM_PACK8: return "Format::RG4_UNORM_PACK8";
    case lux::assetlib::ImageFormat::RGBA4_UNORM_PACK16: return "Format::RGBA4_UNORM_PACK16";
    case lux::assetlib::ImageFormat::BGRA4_UNORM_PACK16: return "Format::BGRA4_UNORM_PACK16";
    case lux::assetlib::ImageFormat::R5G6B5_UNORM_PACK16: return "Format::R5G6B5_UNORM_PACK16";
    case lux::assetlib::ImageFormat::B5G6R5_UNORM_PACK16: return "Format::B5G6R5_UNORM_PACK16";
    case lux::assetlib::ImageFormat::RGB5A1_UNORM_PACK16: return "Format::RGB5A1_UNORM_PACK16";
    case lux::assetlib::ImageFormat::BGR5A1_UNORM_PACK16: return "Format::BGR5A1_UNORM_PACK16";
    case lux::assetlib::ImageFormat::A1RGB5_UNORM_PACK16: return "Format::A1RGB5_UNORM_PACK16";
    case lux::assetlib::ImageFormat::R8_UNORM: return "Format::R8_UNORM";
    case lux::assetlib::ImageFormat::R8_SNORM: return "Format::R8_SNORM";
    case lux::assetlib::ImageFormat::R8_USCALED: return "Format::R8_USCALED";
    case lux::assetlib::ImageFormat::R8_SSCALED: return "Format::R8_SSCALED";
    case lux::assetlib::ImageFormat::R8_UINT: return "Format::R8_UINT";
    case lux::assetlib::ImageFormat::R8_SINT: return "Format::R8_SINT";
    case lux::assetlib::ImageFormat::R8_SRGB: return "Format::R8_SRGB";
    case lux::assetlib::ImageFormat::RG8_UNORM: return "Format::RG8_UNORM";
    case lux::assetlib::ImageFormat::RG8_SNORM: return "Format::RG8_SNORM";
    case lux::assetlib::ImageFormat::RG8_USCALED: return "Format::RG8_USCALED";
    case lux::assetlib::ImageFormat::RG8_SSCALED: return "Format::RG8_SSCALED";
    case lux::assetlib::ImageFormat::RG8_UINT: return "Format::RG8_UINT";
    case lux::assetlib::ImageFormat::RG8_SINT: return "Format::RG8_SINT";
    case lux::assetlib::ImageFormat::RG8_SRGB: return "Format::RG8_SRGB";
    case lux::assetlib::ImageFormat::RGB8_UNORM: return "Format::RGB8_UNORM";
    case lux::assetlib::ImageFormat::RGB8_SNORM: return "Format::RGB8_SNORM";
    case lux::assetlib::ImageFormat::RGB8_USCALED: return "Format::RGB8_USCALED";
    case lux::assetlib::ImageFormat::RGB8_SSCALED: return "Format::RGB8_SSCALED";
    case lux::assetlib::ImageFormat::RGB8_UINT: return "Format::RGB8_UINT";
    case lux::assetlib::ImageFormat::RGB8_SINT: return "Format::RGB8_SINT";
    case lux::assetlib::ImageFormat::RGB8_SRGB: return "Format::RGB8_SRGB";
    case lux::assetlib::ImageFormat::BGR8_UNORM: return "Format::BGR8_UNORM";
    case lux::assetlib::ImageFormat::BGR8_SNORM: return "Format::BGR8_SNORM";
    case lux::assetlib::ImageFormat::BGR8_USCALED: return "Format::BGR8_USCALED";
    case lux::assetlib::ImageFormat::BGR8_SSCALED: return "Format::BGR8_SSCALED";
    case lux::assetlib::ImageFormat::BGR8_UINT: return "Format::BGR8_UINT";
    case lux::assetlib::ImageFormat::BGR8_SINT: return "Format::BGR8_SINT";
    case lux::assetlib::ImageFormat::BGR8_SRGB: return "Format::BGR8_SRGB";
    case lux::assetlib::ImageFormat::RGBA8_UNORM: return "Format::RGBA8_UNORM";
    case lux::assetlib::ImageFormat::RGBA8_SNORM: return "Format::RGBA8_SNORM";
    case lux::assetlib::ImageFormat::RGBA8_USCALED: return "Format::RGBA8_USCALED";
    case lux::assetlib::ImageFormat::RGBA8_SSCALED: return "Format::RGBA8_SSCALED";
    case lux::assetlib::ImageFormat::RGBA8_UINT: return "Format::RGBA8_UINT";
    case lux::assetlib::ImageFormat::RGBA8_SINT: return "Format::RGBA8_SINT";
    case lux::assetlib::ImageFormat::RGBA8_SRGB: return "Format::RGBA8_SRGB";
    case lux::assetlib::ImageFormat::BGRA8_UNORM: return "Format::BGRA8_UNORM";
    case lux::assetlib::ImageFormat::BGRA8_SNORM: return "Format::BGRA8_SNORM";
    case lux::assetlib::ImageFormat::BGRA8_USCALED: return "Format::BGRA8_USCALED";
    case lux::assetlib::ImageFormat::BGRA8_SSCALED: return "Format::BGRA8_SSCALED";
    case lux::assetlib::ImageFormat::BGRA8_UINT: return "Format::BGRA8_UINT";
    case lux::assetlib::ImageFormat::BGRA8_SINT: return "Format::BGRA8_SINT";
    case lux::assetlib::ImageFormat::BGRA8_SRGB: return "Format::BGRA8_SRGB";
    case lux::assetlib::ImageFormat::ABGR8_UNORM_PACK32: return "Format::ABGR8_UNORM_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_SNORM_PACK32: return "Format::ABGR8_SNORM_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_USCALED_PACK32: return "Format::ABGR8_USCALED_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_SSCALED_PACK32: return "Format::ABGR8_SSCALED_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_UINT_PACK32: return "Format::ABGR8_UINT_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_SINT_PACK32: return "Format::ABGR8_SINT_PACK32";
    case lux::assetlib::ImageFormat::ABGR8_SRGB_PACK32: return "Format::ABGR8_SRGB_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_UNORM_PACK32: return "Format::A2RGB10_UNORM_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_SNORM_PACK32: return "Format::A2RGB10_SNORM_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_USCALED_PACK32: return "Format::A2RGB10_USCALED_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_SSCALED_PACK32: return "Format::A2RGB10_SSCALED_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_UINT_PACK32: return "Format::A2RGB10_UINT_PACK32";
    case lux::assetlib::ImageFormat::A2RGB10_SINT_PACK32: return "Format::A2RGB10_SINT_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_UNORM_PACK32: return "Format::A2BGR10_UNORM_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_SNORM_PACK32: return "Format::A2BGR10_SNORM_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_USCALED_PACK32: return "Format::A2BGR10_USCALED_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_SSCALED_PACK32: return "Format::A2BGR10_SSCALED_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_UINT_PACK32: return "Format::A2BGR10_UINT_PACK32";
    case lux::assetlib::ImageFormat::A2BGR10_SINT_PACK32: return "Format::A2BGR10_SINT_PACK32";
    case lux::assetlib::ImageFormat::R16_UNORM: return "Format::R16_UNORM";
    case lux::assetlib::ImageFormat::R16_SNORM: return "Format::R16_SNORM";
    case lux::assetlib::ImageFormat::R16_USCALED: return "Format::R16_USCALED";
    case lux::assetlib::ImageFormat::R16_SSCALED: return "Format::R16_SSCALED";
    case lux::assetlib::ImageFormat::R16_UINT: return "Format::R16_UINT";
    case lux::assetlib::ImageFormat::R16_SINT: return "Format::R16_SINT";
    case lux::assetlib::ImageFormat::R16_FLOAT: return "Format::R16_FLOAT";
    case lux::assetlib::ImageFormat::RG16_UNORM: return "Format::RG16_UNORM";
    case lux::assetlib::ImageFormat::RG16_SNORM: return "Format::RG16_SNORM";
    case lux::assetlib::ImageFormat::RG16_USCALED: return "Format::RG16_USCALED";
    case lux::assetlib::ImageFormat::RG16_SSCALED: return "Format::RG16_SSCALED";
    case lux::assetlib::ImageFormat::RG16_UINT: return "Format::RG16_UINT";
    case lux::assetlib::ImageFormat::RG16_SINT: return "Format::RG16_SINT";
    case lux::assetlib::ImageFormat::RG16_FLOAT: return "Format::RG16_FLOAT";
    case lux::assetlib::ImageFormat::RGB16_UNORM: return "Format::RGB16_UNORM";
    case lux::assetlib::ImageFormat::RGB16_SNORM: return "Format::RGB16_SNORM";
    case lux::assetlib::ImageFormat::RGB16_USCALED: return "Format::RGB16_USCALED";
    case lux::assetlib::ImageFormat::RGB16_SSCALED: return "Format::RGB16_SSCALED";
    case lux::assetlib::ImageFormat::RGB16_UINT: return "Format::RGB16_UINT";
    case lux::assetlib::ImageFormat::RGB16_SINT: return "Format::RGB16_SINT";
    case lux::assetlib::ImageFormat::RGB16_FLOAT: return "Format::RGB16_FLOAT";
    case lux::assetlib::ImageFormat::RGBA16_UNORM: return "Format::RGBA16_UNORM";
    case lux::assetlib::ImageFormat::RGBA16_SNORM: return "Format::RGBA16_SNORM";
    case lux::assetlib::ImageFormat::RGBA16_USCALED: return "Format::RGBA16_USCALED";
    case lux::assetlib::ImageFormat::RGBA16_SSCALED: return "Format::RGBA16_SSCALED";
    case lux::assetlib::ImageFormat::RGBA16_UINT: return "Format::RGBA16_UINT";
    case lux::assetlib::ImageFormat::RGBA16_SINT: return "Format::RGBA16_SINT";
    case lux::assetlib::ImageFormat::RGBA16_FLOAT: return "Format::RGBA16_FLOAT";
    case lux::assetlib::ImageFormat::R32_UINT: return "Format::R32_UINT";
    case lux::assetlib::ImageFormat::R32_SINT: return "Format::R32_SINT";
    case lux::assetlib::ImageFormat::R32_FLOAT: return "Format::R32_FLOAT";
    case lux::assetlib::ImageFormat::RG32_UINT: return "Format::RG32_UINT";
    case lux::assetlib::ImageFormat::RG32_SINT: return "Format::RG32_SINT";
    case lux::assetlib::ImageFormat::RG32_FLOAT: return "Format::RG32_FLOAT";
    case lux::assetlib::ImageFormat::RGB32_UINT: return "Format::RGB32_UINT";
    case lux::assetlib::ImageFormat::RGB32_SINT: return "Format::RGB32_SINT";
    case lux::assetlib::ImageFormat::RGB32_FLOAT: return "Format::RGB32_FLOAT";
    case lux::assetlib::ImageFormat::RGBA32_UINT: return "Format::RGBA32_UINT";
    case lux::assetlib::ImageFormat::RGBA32_SINT: return "Format::RGBA32_SINT";
    case lux::assetlib::ImageFormat::RGBA32_FLOAT: return "Format::RGBA32_FLOAT";
    case lux::assetlib::ImageFormat::R64_UINT: return "Format::R64_UINT";
    case lux::assetlib::ImageFormat::R64_SINT: return "Format::R64_SINT";
    case lux::assetlib::ImageFormat::R64_FLOAT: return "Format::R64_FLOAT";
    case lux::assetlib::ImageFormat::RG64_UINT: return "Format::RG64_UINT";
    case lux::assetlib::ImageFormat::RG64_SINT: return "Format::RG64_SINT";
    case lux::assetlib::ImageFormat::RG64_FLOAT: return "Format::RG64_FLOAT";
    case lux::assetlib::ImageFormat::RGB64_UINT: return "Format::RGB64_UINT";
    case lux::assetlib::ImageFormat::RGB64_SINT: return "Format::RGB64_SINT";
    case lux::assetlib::ImageFormat::RGB64_FLOAT: return "Format::RGB64_FLOAT";
    case lux::assetlib::ImageFormat::RGBA64_UINT: return "Format::RGBA64_UINT";
    case lux::assetlib::ImageFormat::RGBA64_SINT: return "Format::RGBA64_SINT";
    case lux::assetlib::ImageFormat::RGBA64_FLOAT: return "Format::RGBA64_FLOAT";
    case lux::assetlib::ImageFormat::B10G11R11_UFLOAT_PACK32: return "Format::B10G11R11_UFLOAT_PACK32";
    case lux::assetlib::ImageFormat::E5BGR9_UFLOAT_PACK32: return "Format::E5BGR9_UFLOAT_PACK32";
    case lux::assetlib::ImageFormat::D16_UNORM: return "Format::D16_UNORM";
    case lux::assetlib::ImageFormat::X8_D24_UNORM_PACK32: return "Format::X8_D24_UNORM_PACK32";
    case lux::assetlib::ImageFormat::D32_FLOAT: return "Format::D32_FLOAT";
    case lux::assetlib::ImageFormat::S8_UINT: return "Format::S8_UINT";
    case lux::assetlib::ImageFormat::D16_UNORM_S8_UINT: return "Format::D16_UNORM_S8_UINT";
    case lux::assetlib::ImageFormat::D24_UNORM_S8_UINT: return "Format::D24_UNORM_S8_UINT";
    case lux::assetlib::ImageFormat::D32_FLOAT_S8_UINT: return "Format::D32_FLOAT_S8_UINT";
    case lux::assetlib::ImageFormat::BC1_RGB_UNORM_BLOCK: return "Format::BC1_RGB_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC1_RGB_SRGB_BLOCK: return "Format::BC1_RGB_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::BC1_RGBA_UNORM_BLOCK: return "Format::BC1_RGBA_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC1_RGBA_SRGB_BLOCK: return "Format::BC1_RGBA_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::BC2_UNORM_BLOCK: return "Format::BC2_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC2_SRGB_BLOCK: return "Format::BC2_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::BC3_UNORM_BLOCK: return "Format::BC3_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC3_SRGB_BLOCK: return "Format::BC3_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::BC4_UNORM_BLOCK: return "Format::BC4_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC4_SNORM_BLOCK: return "Format::BC4_SNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC5_UNORM_BLOCK: return "Format::BC5_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC5_SNORM_BLOCK: return "Format::BC5_SNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC6H_UFLOAT_BLOCK: return "Format::BC6H_UFLOAT_BLOCK";
    case lux::assetlib::ImageFormat::BC6H_FLOAT_BLOCK: return "Format::BC6H_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::BC7_UNORM_BLOCK: return "Format::BC7_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::BC7_SRGB_BLOCK: return "Format::BC7_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGB8_UNORM_BLOCK: return "Format::ETC2_RGB8_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGB8_SRGB_BLOCK: return "Format::ETC2_RGB8_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGB8A1_UNORM_BLOCK: return "Format::ETC2_RGB8A1_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGB8A1_SRGB_BLOCK: return "Format::ETC2_RGB8A1_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGBA8_UNORM_BLOCK: return "Format::ETC2_RGBA8_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ETC2_RGBA8_SRGB_BLOCK: return "Format::ETC2_RGBA8_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::EAC_R11_UNORM_BLOCK: return "Format::EAC_R11_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::EAC_R11_SNORM_BLOCK: return "Format::EAC_R11_SNORM_BLOCK";
    case lux::assetlib::ImageFormat::EAC_R11G11_UNORM_BLOCK: return "Format::EAC_R11G11_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::EAC_R11G11_SNORM_BLOCK: return "Format::EAC_R11G11_SNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_4x4_UNORM_BLOCK: return "Format::ASTC_4x4_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_4x4_SRGB_BLOCK: return "Format::ASTC_4x4_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x4_UNORM_BLOCK: return "Format::ASTC_5x4_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x4_SRGB_BLOCK: return "Format::ASTC_5x4_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x5_UNORM_BLOCK: return "Format::ASTC_5x5_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x5_SRGB_BLOCK: return "Format::ASTC_5x5_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x5_UNORM_BLOCK: return "Format::ASTC_6x5_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x5_SRGB_BLOCK: return "Format::ASTC_6x5_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x6_UNORM_BLOCK: return "Format::ASTC_6x6_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x6_SRGB_BLOCK: return "Format::ASTC_6x6_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x5_UNORM_BLOCK: return "Format::ASTC_8x5_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x5_SRGB_BLOCK: return "Format::ASTC_8x5_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x6_UNORM_BLOCK: return "Format::ASTC_8x6_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x6_SRGB_BLOCK: return "Format::ASTC_8x6_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x8_UNORM_BLOCK: return "Format::ASTC_8x8_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x8_SRGB_BLOCK: return "Format::ASTC_8x8_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x5_UNORM_BLOCK: return "Format::ASTC_10x5_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x5_SRGB_BLOCK: return "Format::ASTC_10x5_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x6_UNORM_BLOCK: return "Format::ASTC_10x6_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x6_SRGB_BLOCK: return "Format::ASTC_10x6_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x8_UNORM_BLOCK: return "Format::ASTC_10x8_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x8_SRGB_BLOCK: return "Format::ASTC_10x8_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x10_UNORM_BLOCK: return "Format::ASTC_10x10_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x10_SRGB_BLOCK: return "Format::ASTC_10x10_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x10_UNORM_BLOCK: return "Format::ASTC_12x10_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x10_SRGB_BLOCK: return "Format::ASTC_12x10_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x12_UNORM_BLOCK: return "Format::ASTC_12x12_UNORM_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x12_SRGB_BLOCK: return "Format::ASTC_12x12_SRGB_BLOCK";
    case lux::assetlib::ImageFormat::GBGR8_422_UNORM: return "Format::GBGR8_422_UNORM";
    case lux::assetlib::ImageFormat::B8G8RG8_422_UNORM: return "Format::B8G8RG8_422_UNORM";
    case lux::assetlib::ImageFormat::G8_B8_R8_3PLANE_420_UNORM: return "Format::G8_B8_R8_3PLANE_420_UNORM";
    case lux::assetlib::ImageFormat::G8_B8R8_2PLANE_420_UNORM: return "Format::G8_B8R8_2PLANE_420_UNORM";
    case lux::assetlib::ImageFormat::G8_B8_R8_3PLANE_422_UNORM: return "Format::G8_B8_R8_3PLANE_422_UNORM";
    case lux::assetlib::ImageFormat::G8_B8R8_2PLANE_422_UNORM: return "Format::G8_B8R8_2PLANE_422_UNORM";
    case lux::assetlib::ImageFormat::G8_B8_R8_3PLANE_444_UNORM: return "Format::G8_B8_R8_3PLANE_444_UNORM";
    case lux::assetlib::ImageFormat::R10X6_UNORM_PACK16: return "Format::R10X6_UNORM_PACK16";
    case lux::assetlib::ImageFormat::R10X6G10X6_UNORM_2PACK16: return "Format::R10X6G10X6_UNORM_2PACK16";
    case lux::assetlib::ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16: return
            "Format::R10X6G10X6B10X6A10X6_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return
            "Format::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return
            "Format::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return
            "Format::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return
            "Format::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return
            "Format::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return
            "Format::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return
            "Format::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::R12X4_UNORM_PACK16: return "Format::R12X4_UNORM_PACK16";
    case lux::assetlib::ImageFormat::R12X4G12X4_UNORM_2PACK16: return "Format::R12X4G12X4_UNORM_2PACK16";
    case lux::assetlib::ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16: return
            "Format::R12X4G12X4B12X4A12X4_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return
            "Format::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return
            "Format::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return
            "Format::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return
            "Format::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return
            "Format::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return
            "Format::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return
            "Format::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G16B16G16R16_422_UNORM: return "Format::G16B16G16R16_422_UNORM";
    case lux::assetlib::ImageFormat::B16G16RG16_422_UNORM: return "Format::B16G16RG16_422_UNORM";
    case lux::assetlib::ImageFormat::G16_B16_R16_3PLANE_420_UNORM: return "Format::G16_B16_R16_3PLANE_420_UNORM";
    case lux::assetlib::ImageFormat::G16_B16R16_2PLANE_420_UNORM: return "Format::G16_B16R16_2PLANE_420_UNORM";
    case lux::assetlib::ImageFormat::G16_B16_R16_3PLANE_422_UNORM: return "Format::G16_B16_R16_3PLANE_422_UNORM";
    case lux::assetlib::ImageFormat::G16_B16R16_2PLANE_422_UNORM: return "Format::G16_B16R16_2PLANE_422_UNORM";
    case lux::assetlib::ImageFormat::G16_B16_R16_3PLANE_444_UNORM: return "Format::G16_B16_R16_3PLANE_444_UNORM";
    case lux::assetlib::ImageFormat::G8_B8R8_2PLANE_444_UNORM: return "Format::G8_B8R8_2PLANE_444_UNORM";
    case lux::assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return
            "Format::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return
            "Format::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
    case lux::assetlib::ImageFormat::G16_B16R16_2PLANE_444_UNORM: return "Format::G16_B16R16_2PLANE_444_UNORM";
    case lux::assetlib::ImageFormat::A4RGB4_UNORM_PACK16: return "Format::A4RGB4_UNORM_PACK16";
    case lux::assetlib::ImageFormat::A4B4G4R4_UNORM_PACK16: return "Format::A4B4G4R4_UNORM_PACK16";
    case lux::assetlib::ImageFormat::ASTC_4x4_FLOAT_BLOCK: return "Format::ASTC_4x4_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x4_FLOAT_BLOCK: return "Format::ASTC_5x4_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_5x5_FLOAT_BLOCK: return "Format::ASTC_5x5_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x5_FLOAT_BLOCK: return "Format::ASTC_6x5_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_6x6_FLOAT_BLOCK: return "Format::ASTC_6x6_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x5_FLOAT_BLOCK: return "Format::ASTC_8x5_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x6_FLOAT_BLOCK: return "Format::ASTC_8x6_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_8x8_FLOAT_BLOCK: return "Format::ASTC_8x8_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x5_FLOAT_BLOCK: return "Format::ASTC_10x5_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x6_FLOAT_BLOCK: return "Format::ASTC_10x6_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x8_FLOAT_BLOCK: return "Format::ASTC_10x8_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_10x10_FLOAT_BLOCK: return "Format::ASTC_10x10_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x10_FLOAT_BLOCK: return "Format::ASTC_12x10_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::ASTC_12x12_FLOAT_BLOCK: return "Format::ASTC_12x12_FLOAT_BLOCK";
    case lux::assetlib::ImageFormat::A1BGR5_UNORM_PACK16: return "Format::A1BGR5_UNORM_PACK16";
    case lux::assetlib::ImageFormat::A8_UNORM: return "Format::A8_UNORM";
    case lux::assetlib::ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG: return "Format::PVRTC1_2BPP_UNORM_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG: return "Format::PVRTC1_4BPP_UNORM_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG: return "Format::PVRTC2_2BPP_UNORM_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG: return "Format::PVRTC2_4BPP_UNORM_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG: return "Format::PVRTC1_2BPP_SRGB_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG: return "Format::PVRTC1_4BPP_SRGB_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG: return "Format::PVRTC2_2BPP_SRGB_BLOCK_IMG";
    case lux::assetlib::ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG: return "Format::PVRTC2_4BPP_SRGB_BLOCK_IMG";
    case lux::assetlib::ImageFormat::R8_BOOL_ARM: return "Format::R8_BOOL_ARM";
    case lux::assetlib::ImageFormat::RG16_SFIXED5_NV: return "Format::RG16_SFIXED5_NV";
    case lux::assetlib::ImageFormat::R10X6_UINT_PACK16_ARM: return "Format::R10X6_UINT_PACK16_ARM";
    case lux::assetlib::ImageFormat::R10X6G10X6_UINT_2PACK16_ARM: return "Format::R10X6G10X6_UINT_2PACK16_ARM";
    case lux::assetlib::ImageFormat::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM: return
            "Format::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM";
    case lux::assetlib::ImageFormat::R12X4_UINT_PACK16_ARM: return "Format::R12X4_UINT_PACK16_ARM";
    case lux::assetlib::ImageFormat::R12X4G12X4_UINT_2PACK16_ARM: return "Format::R12X4G12X4_UINT_2PACK16_ARM";
    case lux::assetlib::ImageFormat::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM: return
            "Format::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2_UINT_PACK16_ARM: return "Format::R14X2_UINT_PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2G14X2_UINT_2PACK16_ARM: return "Format::R14X2G14X2_UINT_2PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM: return
            "Format::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2_UNORM_PACK16_ARM: return "Format::R14X2_UNORM_PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2G14X2_UNORM_2PACK16_ARM: return "Format::R14X2G14X2_UNORM_2PACK16_ARM";
    case lux::assetlib::ImageFormat::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM: return
            "Format::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM";
    case lux::assetlib::ImageFormat::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM: return
            "Format::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM";
    case lux::assetlib::ImageFormat::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM: return
            "Format::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM";
    default:
        ASSERT(false)
        return "Format::Undefined";
    }
}

struct Writer
{
    static constexpr u32 INDENT_SPACES = 4;

    struct CountInfo
    {
        u32 Buffers{0};
        u32 Images{0};
        u32 Samplers{0};
    };

    std::stringstream Stream;
    u32 IndentLevel{0};
    CountInfo Counts{};
    std::optional<lux::assetlib::io::IoError> Error;

    void Push()
    {
        IndentLevel += INDENT_SPACES;
    }

    void Pop()
    {
        ASSERT(IndentLevel >= INDENT_SPACES)
        IndentLevel -= INDENT_SPACES;
    }

    void WriteIndent()
    {
        Stream << std::string(IndentLevel, ' ');
    }

    void WriteLine(std::string_view line)
    {
        WriteIndent();
        Stream << line << "\n";
    }

    void AddError(const std::string& message)
    {
        if (!Error.has_value())
            Error = {.Code = lux::assetlib::io::IoError::ErrorCode::GeneralError, .Message = {}};
        Error->Message.append(message).append("\n");
    }

    void BeginSetResourceFunction(const BindingsInfo::Signature& signature)
    {
        std::string line = std::format("RG::Resource Set{}(RG::Resource resource",
            utils::canonicalizeName(signature.Name));
        if (signature.Count > 1)
            line.append(", u32 index");
        line.append(", RG::ResourceAccessFlags additionalAccess = RG::ResourceAccessFlags::None)");
        WriteLine(line);
        WriteLine("{");
        Push();
        WriteLine("using enum RG::ResourceAccessFlags;");
    }

    void BeginSetUniformFunction(const UniformBindingsInfo::Signature& signature)
    {
        std::string line = std::format("RG::Resource Set{}(const {}& {}",
            utils::canonicalizeName(signature.BindingSignature.Name), signature.Type, signature.Parameter);
        if (signature.BindingSignature.Count > 1)
            line.append(", u32 index");
        line.append(", RG::ResourceAccessFlags additionalAccess = RG::ResourceAccessFlags::None)");
        WriteLine(line);
        WriteLine("{");
        Push();
        WriteLine("using enum RG::ResourceAccessFlags;");
    }

    void EndSetResourceFunction()
    {
        WriteLine("return resource;");
        Pop();
        WriteLine("}");
    }

    bool WriteResourceAccess(const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string shaderAccess = shaderStagesToGraphUsage(access.ShaderStages);
        const std::string graphUsage = bindingTypeToGraphUsage(access.Type);
        if (shaderAccess.empty() || graphUsage.empty())
        {
            AddError(std::format("Failed to infer usage for binding {}", signature.Name));
            return false;
        }
        WriteLine(std::format("resource = Graph->{}{}(resource, {} | {} | additionalAccess);",
            accessToString(access.Access), bindingTypeToString(access.Type), shaderAccess, graphUsage));

        return true;
    }

    static std::string GetBindingInfoContentString(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        return std::format(
            ".Set = {}, "
            ".Slot = {}, "
            ".DescriptorType = {}, "
            ".Resource = resource, "
            ".ArrayOffset = {}",
            set,
            access.Binding,
            bindingTypeToDescriptorTypeString(access.Type),
            signature.Count > 1 ? "index" : "0");
    }

    void WriteOrdinaryImage(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string line = std::format("m_ImageBindings[m_ImageCount] = ImageBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
        WriteLine("m_ImageCount += 1;");
        Counts.Images += signature.Count;
    }

    void WriteOrdinaryBuffer(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string line = std::format("m_BufferBindings[m_BufferCount] = BufferBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
        WriteLine("m_BufferCount += 1;");
        Counts.Buffers += signature.Count;
    }

    bool OnlyMainVariant(const std::vector<std::string>& variants)
    {
        return
            variants.empty() ||
            variants.size() == 1 && variants.front() == lux::assetlib::ShaderLoadInfo::SHADER_VARIANT_MAIN_NAME;
    }

    void WriteVariantsEnum(const std::vector<std::string>& variants)
    {
        if (OnlyMainVariant(variants))
            return;
        WriteLine("enum class Variants : u8");
        WriteLine("{");
        Push();
        for (auto& variant : variants)
            WriteLine(std::format("{},", utils::canonicalizeName(variant)));
        Pop();
        WriteLine("};");
    }

    void WriteConstructor(std::string_view structName, std::string_view shaderName,
        const std::vector<std::string>& variants)
    {
        WriteLine(std::format("{}() = default;", structName));

        if (OnlyMainVariant(variants))
        {
            WriteLine(std::format("{}(RG::Graph& graph)", structName));
            WriteLine(std::format("\t: BindGroupBaseRG(graph, graph.SetShader(\"{}\"_hsv)) {{}}", shaderName));
            WriteLine(std::format("{}(RG::Graph& graph, ShaderOverridesView&& overrides)", structName));
            WriteLine(std::format("\t: BindGroupBaseRG(graph, graph.SetShader(\"{}\"_hsv, std::move(overrides))) {{}}",
                shaderName));
            return;
        }

        WriteLine(std::format("{}(RG::Graph& graph, Variants variant)", structName));
        WriteLine("\t: BindGroupBaseRG(graph), m_Variant(variant)");
        WriteLine("{");
        Push();
        WriteLine("switch (m_Variant)");
        WriteLine("{");
        for (auto& variant : variants)
        {
            WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variant)));
            Push();
            WriteLine(std::format(R"(Shader = &graph.SetShader("{}"_hsv, "{}"_hsv);)", shaderName, variant));
            WriteLine("break;");
            Pop();
        }
        WriteLine("}");
        Pop();
        WriteLine("}");

        WriteLine(std::format("{}(RG::Graph& graph, Variants variant, ShaderOverridesView&& overrides)", structName));
        WriteLine("\t: BindGroupBaseRG(graph), m_Variant(variant)");
        WriteLine("{");
        Push();
        WriteLine("switch (m_Variant)");
        WriteLine("{");
        for (auto& variant : variants)
        {
            WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variant)));
            Push();
            WriteLine(std::format(R"(Shader = &graph.SetShader("{}"_hsv, "{}"_hsv, std::move(overrides));)",
                shaderName, variant));
            WriteLine("break;");
            Pop();
        }
        WriteLine("}");
        Pop();
        WriteLine("}");
    }

    void WriteVariantEnumField(const std::vector<std::string>& variants)
    {
        if (OnlyMainVariant(variants))
            return;
        WriteLine(std::format("Variants m_Variant{{Variants::{}}};", utils::canonicalizeName(variants.front())));
    }

    void WriteEmbeddedUniformTypes(const std::string& uniform)
    {
        std::stringstream ss(uniform);
        std::string line;
        while (std::getline(ss, line))
            WriteLine(line);
    }

    bool ValidateAccessVariants(const BindingsInfo& bindings, const BindingsInfo::Signature& signature)
    {
        const bool hasSamplers = bindings.HasSamplers(signature);
        const bool onlySamplers = bindings.OnlySamplers(signature);

        if (hasSamplers && !onlySamplers)
        {
            AddError("Sampler-Resource divergence is not supported");
            return false;
        }

        return true;
    }

    void BeginDivergenceSwitch(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("switch (m_Variant)");
            WriteLine("{");
            Push();
        }
    }

    void EndDivergenceSwitch(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("default:");
            Push();
            WriteLine("break;");
            Pop();
            Pop();
            WriteLine("}");
        }
    }

    void BeginDivergenceCase(bool isDivergent, const std::vector<u32> accessVariants,
        const std::vector<std::string>& variants)
    {
        if (isDivergent)
        {
            for (auto& variant : accessVariants)
                WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variants[variant])));
            Push();
        }
    }

    void EndDivergenceCase(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("break;");
            Pop();
        }
    }

    void WriteSamplerBindings(u32 set, const BindingsInfo::Signature& signature,
        const std::vector<BindingsInfo::AccessVariant>& accesses, const std::vector<std::string>& variants,
        bool isDivergent)
    {
        if (signature.Count > 1)
        {
            AddError(std::format("Array of samplers are not supported: {}", signature.Name));
            return;
        }

        const bool isImmutableAcrossDivergence =
            !isDivergent && enumHasAny(accesses.front().Attributes,
                lux::assetlib::ShaderBindingAttributes::ImmutableSampler) ||
            std::ranges::all_of(accesses, [](auto& access)
            {
                return enumHasAny(access.Attributes, lux::assetlib::ShaderBindingAttributes::ImmutableSampler);
            });

        if (isImmutableAcrossDivergence)
            return;

        WriteLine(std::format("void Set{}(Sampler sampler)", utils::canonicalizeName(signature.Name)));
        WriteLine("{");
        Push();

        BeginDivergenceSwitch(isDivergent);
        for (auto& access : accesses)
        {
            BeginDivergenceCase(isDivergent, access.Variants, variants);

            WriteLine(std::format("m_SamplerBindings[m_SamplerCount] = SamplerBindingInfoRG{{"
                ".Set = {}, "
                ".Slot = {}, "
                ".Sampler = sampler}};",
                set,
                access.Binding
            ));
            WriteLine("m_SamplerCount += 1;");

            EndDivergenceCase(isDivergent);
        }
        EndDivergenceSwitch(isDivergent);

        Pop();
        WriteLine("}");
        Counts.Samplers += signature.Count;
    }

    void WriteResourceBindingsFunctionBody(u32 set,
        const BindingsInfo::Signature& signature, const std::vector<BindingsInfo::AccessVariant>& accesses,
        const std::vector<std::string>& variants, bool isDivergent)
    {
        BeginDivergenceSwitch(isDivergent);
        for (auto& access : accesses)
        {
            BeginDivergenceCase(isDivergent, access.Variants, variants);

            const bool success = WriteResourceAccess(signature, access);
            if (success)
            {
                if (isBuffer(access.Type))
                    WriteOrdinaryBuffer(set, signature, access);
                else
                    WriteOrdinaryImage(set, signature, access);
            }

            EndDivergenceCase(isDivergent);
        }
        EndDivergenceSwitch(isDivergent);
    }

    void WriteResourceBindings(u32 set, const BindingsInfo::Signature& signature,
        const std::vector<BindingsInfo::AccessVariant>& accesses, const std::vector<std::string>& variants,
        bool isDivergent)
    {
        BeginSetResourceFunction(signature);
        WriteResourceBindingsFunctionBody(set, signature, accesses, variants, isDivergent);
        EndSetResourceFunction();
    }

    void WriteBindings(u32 set, const BindingsInfo& bindings,
        const std::vector<std::string>& variants)
    {
        /* we don't need to generate explicit bindings for texture heap */
        if (set == lux::assetlib::SHADER_TEXTURE_HEAP_DESCRIPTOR_SET_INDEX)
            return;

        for (auto&& [signature, accessVariants] : bindings.Bindings)
        {
            const bool isValid = ValidateAccessVariants(bindings, signature);
            if (!isValid)
            {
                AddError(std::format("Validate binding failed for set {}", set));
                continue;
            }

            const bool isDivergent = bindingAccessIsDivergent(accessVariants, variants);

            if (bindings.OnlySamplers(signature))
            {
                WriteSamplerBindings(set, signature, accessVariants, variants, isDivergent);
                continue;
            }

            WriteResourceBindings(set, signature, accessVariants, variants, isDivergent);
        }
    }

    void WriteUniformBindings(u32 set, const UniformBindingsInfo& uniforms,
        const BindingsInfo& bindings, const std::vector<std::string>& variants)
    {
        /* we don't need to generate explicit bindings for texture heap */
        if (set == lux::assetlib::SHADER_TEXTURE_HEAP_DESCRIPTOR_SET_INDEX)
            return;

        if (uniforms.Bindings.empty())
            return;

        if (uniforms.Bindings.size() > 1)
        {
            AddError("Divergent uniforms are not supported");
            return;
        }

        for (auto&& [uniformSignature, _] : uniforms.Bindings)
        {
            BeginSetUniformFunction(uniformSignature);

            WriteLine(std::format("RG::Resource resource = Graph->Create(\"{}\"_hsv, RG::RGBufferDescription{{"
                ".SizeBytes = sizeof({})}});", uniformSignature.Type, uniformSignature.Type));
            WriteLine(std::format("resource = Graph->Upload(resource, {});", uniformSignature.Parameter));

            auto& bindingSignature = uniformSignature.BindingSignature;
            auto& bindingAccesses = bindings.Bindings.at(bindingSignature);
            WriteResourceBindingsFunctionBody(set, bindingSignature, bindingAccesses, variants,
                bindingAccessIsDivergent(bindingAccesses, variants));

            EndSetResourceFunction();
        }
    }

    void WriteRasterizationInfo(const lux::assetlib::ShaderLoadRasterizationInfo& rasterization)
    {
        for (auto& color : rasterization.Colors)
            WriteLine(std::format("static Format Get{}AttachmentFormat() {{ return {}; }}",
                utils::canonicalizeName(color.Name), formatFromImageFormat(color.Format)));
        if (rasterization.Depth.has_value())
            WriteLine(std::format("static Format GetDepthFormat() {{ return {}; }}",
                formatFromImageFormat(*rasterization.Depth)));
    }

    void WriteGroupSizeInfo(const std::vector<lux::assetlib::ShaderEntryPoint>& entryPoints)
    {
        for (auto& entry : entryPoints)
        {
            if (entry.ShaderStage != lux::assetlib::ShaderStage::Compute)
                continue;

            WriteLine(std::format("static glm::uvec3 Get{}GroupSize() {{ return glm::uvec3{{{}, {}, {}}}; }}",
                utils::canonicalizeName(entry.Name),
                entry.ThreadGroupSize.x, entry.ThreadGroupSize.y, entry.ThreadGroupSize.z));
        }
    }

    void BeginForLoop(std::string_view count)
    {
        WriteLine(std::format("for (u32 i = 0; i < {}; i++)", count));
        WriteLine("{");
        Push();
    }

    void EndForLoop()
    {
        Pop();
        WriteLine("}");
    }

    void WriteBeginPrivate()
    {
        Pop();
        WriteLine("");
        WriteLine("private:");
        Push();
    }

    void WriteBindDescriptorsType(const std::vector<BindingSetsInfo>& bindingSets,
        const std::vector<std::string>& variants, const std::string& type)
    {
        WriteLine(std::format(
            "void Bind{}Descriptors(RenderCommandList& cmdList) const",
            type));
        WriteLine("{");
        Push();
        if (Counts.Samplers > 0)
        {
            BeginForLoop("m_SamplerCount");
            WriteLine("const auto& binding = m_SamplerBindings[i];");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = DescriptorType::Sampler}, binding.Sampler);");
            EndForLoop();
        }
        if (Counts.Buffers > 0)
        {
            BeginForLoop("m_BufferCount");
            WriteLine("const auto& binding = m_BufferBindings[i];");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "Graph->GetBufferBinding(binding.Resource), binding.ArrayOffset);");
            EndForLoop();
        }
        if (Counts.Images > 0)
        {
            BeginForLoop("m_ImageCount");
            WriteLine("const auto& binding = m_ImageBindings[i];");
            WriteLine("const auto& image = Graph->GetImageBinding(binding.Resource);");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "image.Subresource, image.Layout, binding.ArrayOffset);");
            EndForLoop();
        }
        for (auto& set : bindingSets)
        {
            const bool isDivergent = set.Sets.size() > 1 || set.Sets.begin()->second.size() != variants.size();
            BeginDivergenceSwitch(isDivergent);
            for (auto&& [signature, accessVariants] : set.Sets)
            {
                BeginDivergenceCase(isDivergent, accessVariants, variants);
                if (signature.HasImmutableSamplers)
                    WriteLine(std::format(
                        "cmdList.BindImmutableSamplers{}({{"
                        ".Descriptors = Shader->Descriptors({}), .PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                        type, signature.Set, signature.Set));
                else
                    WriteLine(std::format(
                        "cmdList.BindDescriptors{}({{"
                        ".Descriptors = Shader->Descriptors({}), "
                        ".PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                        type, signature.Set, signature.Set));
                EndDivergenceCase(isDivergent);
            }
            EndDivergenceSwitch(isDivergent);
        }
        Pop();
        WriteLine("}");
    }

    void WriteBindPipelineType(const std::string& type)
    {
        WriteLine(std::format(
            "void Bind{}(RenderCommandList& cmdList) const",
            type));
        WriteLine("{");
        Push();
        WriteLine(std::format("cmdList.BindPipeline{}({{.Pipeline = Shader->Pipeline()}});", type));
        WriteLine(std::format("Bind{}Descriptors(cmdList);", type));
        Pop();
        WriteLine("}");
    }

    void WriteResourceContainers()
    {
        if (Counts.Samplers > 0)
        {
            WriteLine(std::format("std::array<SamplerBindingInfoRG, {}> m_SamplerBindings{{}};", Counts.Samplers));
            WriteLine("u32 m_SamplerCount{};");
        }
        if (Counts.Buffers > 0)
        {
            WriteLine(std::format("std::array<BufferBindingInfoRG, {}> m_BufferBindings{{}};", Counts.Buffers));
            WriteLine("u32 m_BufferCount{};");
        }
        if (Counts.Images > 0)
        {
            WriteLine(std::format("std::array<ImageBindingInfoRG, {}> m_ImageBindings{{}};", Counts.Images));
            WriteLine("u32 m_ImageCount{};");
        }
    }
};
}

SlangGenerator::SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator,
    const std::filesystem::path& initialDirectory, lux::assetlib::io::AssetIoInterface& io)
    : m_UniformTypeGenerator(&uniformTypeGenerator), m_InitialDirectory(initialDirectory), m_Io(&io)
{
}

std::string SlangGenerator::GenerateCommonFile() const
{
    std::string commonFile = std::string(utils::getPreamble()).append("\n");
    commonFile += R"(
#include "Assets/Shaders/ShaderAssetManager.h"
#include "RenderGraph/RGGraph.h"
#include "Rendering/Commands/RenderCommandList.h"
#include "FrameContext.h"

struct BindGroupBaseRG
{
    struct SamplerBindingInfoRG
    {
        u32 Set{0};
        u32 Slot{0};
        Sampler Sampler{};
    };
    struct BindingInfoRG
    {
        u32 Set{0};
        u32 Slot{0};
        DescriptorType DescriptorType{};
        RG::Resource Resource{};
        u32 ArrayOffset{0};
    };
    using BufferBindingInfoRG = BindingInfoRG;
    using ImageBindingInfoRG = BindingInfoRG;

    BindGroupBaseRG() = default;
    BindGroupBaseRG(RG::Graph& graph, const ::lux::ShaderAsset& shader)
        : Graph(&graph), Shader(&shader) {}
    BindGroupBaseRG(RG::Graph& graph)
        : Graph(&graph) {}

    RG::Graph* Graph{nullptr};
    const ::lux::ShaderAsset* Shader{nullptr};
};
)";

    return commonFile;
}

namespace
{
constexpr std::string_view GENERATED_COMMON_FILE_NAME = "BindGroupBaseRG.generated.h";
}

std::filesystem::path SlangGenerator::GetCommonFilePath(const std::filesystem::path& generationPath) const
{
    return generationPath / GENERATED_COMMON_FILE_NAME;
}

lux::assetlib::io::IoResult<SlangGeneratorResult> SlangGenerator::Generate(const std::filesystem::path& path) const
{
    auto shaderLoadInfo = lux::assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return std::unexpected(shaderLoadInfo.error());

    std::vector<std::string> variants;
    std::unordered_set<std::filesystem::path> standaloneUniforms;
    std::unordered_map<lux::assetlib::AssetId, std::string> embeddedStructs;

    std::vector<BindingSetsInfo> bindingsSetsInfoPerSet;
    std::vector<BindingsInfo> bindingsInfoPerSet;
    std::vector<UniformBindingsInfo> uniformBindingsInfoPerSet;
    std::vector<lux::assetlib::ShaderEntryPoint> entryPoints;

    for (auto& variant : shaderLoadInfo->Variants)
    {
        const u32 variantIndex = (u32)variants.size();
        variants.push_back(variant.Name);

        const std::filesystem::path bakedPath =
            lux::bakers::Slang::GetBakedPath(path, StringId::FromString(variant.Name), {},
                {.InitialDirectory = m_InitialDirectory});

        auto assetFileResult = m_Io->ReadHeader(bakedPath);
        if (!assetFileResult.has_value())
            return std::unexpected(assetFileResult.error());

        auto shaderUnpack = lux::assetlib::shader::readHeader(*assetFileResult);
        if (!shaderUnpack.has_value())
            return std::unexpected(shaderUnpack.error());

        const lux::assetlib::ShaderHeader& shader = *shaderUnpack;

        if (entryPoints.empty())
            entryPoints = shader.EntryPoints;

        for (auto& set : shader.BindingSets)
        {
            if (set.Set >= bindingsSetsInfoPerSet.size())
                bindingsSetsInfoPerSet.resize(set.Set + 1);
            bindingsSetsInfoPerSet[set.Set].Add(
                {
                    .HasImmutableSamplers = std::ranges::any_of(set.Bindings,
                        [](const lux::assetlib::ShaderBinding& binding)
                        {
                            return enumHasAny(binding.Attributes,
                                lux::assetlib::ShaderBindingAttributes::ImmutableSampler);
                        }),
                    .Set = set.Set
                }, variantIndex);

            if (set.Set >= bindingsInfoPerSet.size())
                bindingsInfoPerSet.resize(set.Set + 1);
            for (auto& binding : set.Bindings)
                bindingsInfoPerSet[set.Set].AddAccessVariant(
                    {
                        .Name = binding.Name,
                        .Count = binding.Count
                    },
                    {
                        .Variants = {variantIndex},
                        .ShaderStages = binding.ShaderStages,
                        .Binding = binding.Binding,
                        .Type = binding.Type,
                        .Access = binding.Access,
                        .Attributes = binding.Attributes
                    });

            if (set.UniformType.empty())
                continue;
            auto uniformResult = m_UniformTypeGenerator->Generate(set.UniformType);
            if (!uniformResult.has_value())
                return std::unexpected(uniformResult.error());

            auto& uniform = *uniformResult;
            for (auto&& [id, embedded] : uniform.EmbeddedStructs)
                embeddedStructs.emplace(id, embedded);

            if (uniform.IsStandalone)
                for (auto& include : uniform.Includes)
                    standaloneUniforms.insert(std::move(include));

            if (set.Set >= uniformBindingsInfoPerSet.size())
                uniformBindingsInfoPerSet.resize(set.Set + 1);

            uniformBindingsInfoPerSet[set.Set].Add(
                {
                    .Type = uniform.TypeName,
                    .Parameter = uniform.ParameterName,
                    .BindingSignature =
                    {
                        .Name = set.Bindings.front().Name,
                        .Count = set.Bindings.front().Count
                    }
                }, variantIndex);
        }
    }

    std::string generatedStructName = std::format("{}BindGroupRG", utils::canonicalizeName(shaderLoadInfo->Name));
    std::string generatedFileName = std::format("{}.generated.h", generatedStructName);

    Writer writer;

    writer.WriteLine(utils::getPreamble());
    writer.WriteLine(std::format("#include \"{}\"", GENERATED_COMMON_FILE_NAME));
    for (auto& standaloneUniform : standaloneUniforms)
        writer.WriteLine(std::format("#include \"{}\"", standaloneUniform.string()));
    writer.WriteLine("#include <array>");
    writer.WriteLine("");
    writer.WriteLine(std::format("struct {} : BindGroupBaseRG", generatedStructName));
    writer.WriteLine("{");
    writer.Push();
    writer.WriteVariantsEnum(variants);
    writer.WriteConstructor(generatedStructName, shaderLoadInfo->Name, variants);
    for (auto& embedded : embeddedStructs | std::views::values)
        writer.WriteEmbeddedUniformTypes(embedded);
    for (u32 i = 0; i < uniformBindingsInfoPerSet.size(); i++)
        writer.WriteUniformBindings(i, uniformBindingsInfoPerSet[i], bindingsInfoPerSet[i], variants);
    for (u32 i = 0; i < bindingsInfoPerSet.size(); i++)
        writer.WriteBindings(i, bindingsInfoPerSet[i], variants);
    if (shaderLoadInfo->RasterizationInfo.has_value())
        writer.WriteRasterizationInfo(*shaderLoadInfo->RasterizationInfo);
    writer.WriteGroupSizeInfo(entryPoints);

    bool hasGraphics = false;
    bool hasCompute = false;
    for (auto& entry : entryPoints)
    {
        hasGraphics = hasGraphics || entry.ShaderStage != lux::assetlib::ShaderStage::Compute;
        hasCompute = hasCompute || entry.ShaderStage == lux::assetlib::ShaderStage::Compute;
    }

    if (hasGraphics)
        writer.WriteBindPipelineType("Graphics");
    if (hasCompute)
        writer.WriteBindPipelineType("Compute");

    writer.WriteBeginPrivate();
    if (hasGraphics)
        writer.WriteBindDescriptorsType(bindingsSetsInfoPerSet, variants, "Graphics");
    if (hasCompute)
        writer.WriteBindDescriptorsType(bindingsSetsInfoPerSet, variants, "Compute");
    writer.WriteVariantEnumField(variants);
    writer.WriteResourceContainers();
    writer.Pop();
    writer.WriteLine("};");

    return SlangGeneratorResult{
        .Generated = writer.Stream.str(),
        .FileName = generatedFileName
    };
}
