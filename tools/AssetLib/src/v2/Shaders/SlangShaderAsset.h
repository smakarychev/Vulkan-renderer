#pragma once
#include "core.h"
#include "v2/AssetLibV2.h"

#include <glm/glm.hpp>

namespace assetlib
{
enum class ShaderStage : u8
{
    None = 0,
    Vertex = BIT(0),
    Pixel = BIT(1),
    Compute = BIT(2),
};

enum class ShaderBindingType : u8
{
    None = 0,
    Sampler,
    Image,
    ImageStorage,
    TexelUniform,
    TexelStorage,
    UniformBuffer,
    UniformTexelBuffer,
    StorageBuffer,
    StorageTexelBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    Input,
};

enum class ShaderBindingAccess : u8
{
    Read,
    Write,
    ReadWrite,
};

enum class ShaderBindingAttributes : u64
{
    None = 0,
    Bindless = BIT(0),
    ImmutableSampler = BIT(1),
    ImmutableSamplerNearest = BIT(2),
    ImmutableSamplerClampEdge = BIT(3),
    ImmutableSamplerClampBlack = BIT(4),
    ImmutableSamplerClampWhite = BIT(5),
    ImmutableSamplerShadow = BIT(6),
    ImmutableSamplerReductionMin = BIT(7),
    ImmutableSamplerReductionMax = BIT(8),
};

struct ShaderBinding
{
    std::string Name;
    ShaderStage ShaderStages{};
    u32 Count{1};
    u32 Binding{0};
    ShaderBindingType Type{ShaderBindingType::None};
    ShaderBindingAccess Access{ShaderBindingAccess::Read};
    ShaderBindingAttributes Attributes{ShaderBindingAttributes::None};
};

struct ShaderBindingSet
{
    u32 Set{0};
    std::vector<ShaderBinding> Bindings;
    std::string UniformType{};
};

struct ShaderPushConstant
{
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage ShaderStages{};
};

enum class ShaderScalarType : u8
{
    None = 0,
    Void,
    Bool,
    I32,
    U32,
    I64,
    U64,
    F16,
    F32,
    F64,
    I8,
    U8,
    I16,
    U16,
};

struct ShaderInputAttribute
{
    std::string Name{};
    u32 Location{};
    u32 ElementCount{};
    ShaderScalarType ElementScalar{ShaderScalarType::None};
};

struct ShaderSpecializationConstants
{
    std::string Name{};
    u32 Id{};
    ShaderScalarType Type{ShaderScalarType::None};
    ShaderStage ShaderStages{};
};

struct ShaderEntryPoint
{
    std::string Name{};
    ShaderStage ShaderStage{};
    glm::uvec3 ThreadGroupSize{};
};

struct ShaderHeader
{
    std::string Name{};
    std::vector<ShaderEntryPoint> EntryPoints{};
    std::vector<ShaderBindingSet> BindingSets{};
    ShaderPushConstant PushConstant{};
    std::vector<ShaderSpecializationConstants> SpecializationConstants{};
    std::vector<ShaderInputAttribute> InputAttributes{};
    std::vector<std::string> Includes;
};

struct ShaderAsset
{
    ShaderHeader Header{};
    std::vector<std::byte> Spirv;
};

namespace shader
{
AssetMetadata generateMetadata(std::string_view fileName);

io::IoResult<ShaderHeader> unpackHeader(const AssetFile& assetFile);
io::IoResult<AssetBinary> unpackBinary(const AssetFile& assetFile, const AssetBinary& assetBinary);

io::IoResult<AssetCustomHeaderType> packHeader(const ShaderHeader& shaderHeader);
AssetBinary packBinary(AssetBinary& spirv, CompressionMode compressionMode);
}
}

CREATE_ENUM_FLAGS_OPERATORS(assetlib::ShaderStage);
CREATE_ENUM_FLAGS_OPERATORS(assetlib::ShaderBindingAttributes);
