#pragma once
#include "core.h"
#include "v2/Io/AssetIo.h"

#include <glm/glm.hpp>

namespace lux::assetlib
{
namespace io
{
class AssetIoInterface;
class AssetCompressor;
}

static constexpr u32 SHADER_TEXTURE_HEAP_DESCRIPTOR_SET_INDEX = 2;
static constexpr u32 SHADER_TEXTURE_HEAP_DESCRIPTOR_SET_BINDING_INDEX = 2;

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
    BindlessHandle = BIT(1),
    ImmutableSampler = BIT(2),
    ImmutableSamplerNearest = BIT(3),
    ImmutableSamplerClampEdge = BIT(4),
    ImmutableSamplerClampBlack = BIT(5),
    ImmutableSamplerClampWhite = BIT(6),
    ImmutableSamplerShadow = BIT(7),
    ImmutableSamplerReductionMin = BIT(8),
    ImmutableSamplerReductionMax = BIT(9),
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
    /*
     * generally, every attribute has its own binding
     * structs have implicit behaviour: every field in the struct is assumed to be the same binding slot
     * this way, it is possible to describe both interleaved and separated input arguments
     */
    u32 Binding{};
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
io::IoResult<ShaderHeader> readHeader(const AssetFile& assetFile);
io::IoResult<std::vector<std::byte>> readSpirv(const ShaderHeader& header, const AssetFile& assetFile,
    io::AssetIoInterface& io, io::AssetCompressor& compressor);
io::IoResult<ShaderAsset> readShader(const AssetFile& assetFile,
    io::AssetIoInterface& io, io::AssetCompressor& compressor);

io::IoResult<AssetPacked> pack(const ShaderAsset& shader, io::AssetCompressor& compressor);
}
}

CREATE_ENUM_FLAGS_OPERATORS(lux::assetlib::ShaderStage);
CREATE_ENUM_FLAGS_OPERATORS(lux::assetlib::ShaderBindingAttributes);
