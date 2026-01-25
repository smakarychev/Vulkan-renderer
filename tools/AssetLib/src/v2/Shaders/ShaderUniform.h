#pragma once

#include "ShaderAsset.h"

#include <memory>
#include <variant>

namespace lux::assetlib
{

static constexpr std::string_view SHADER_UNIFORM_TYPE_EXTENSION = ".type";

enum class ShaderUniformTypeKind : u8
{
    None = 0,
    Struct,
    Array,
    Matrix,
    Vector,
    Scalar,
    ConstantBuffer,
    Resource,
    SamplerState,
    TextureBuffer,
    ShaderStorageBuffer,
    ParameterBlock,
    GenericTypeParameter,
    Interface,
    OutputStream,
    Specialized,
    Feedback,
    Pointer,
    DynamicResource,
};

struct ShaderUniformTypeScalar
{
    ShaderScalarType Scalar{ShaderScalarType::None};
};

struct ShaderUniformTypeVector
{
    u32 Elements{1};
    ShaderScalarType Scalar{ShaderScalarType::None};
};

struct ShaderUniformTypeMatrix
{
    u32 Rows{1};
    u32 Columns{1};
    ShaderScalarType Scalar{ShaderScalarType::None};
};

struct ShaderUniformTypeArray;
struct ShaderUniformTypeStruct;
struct ShaderUniformTypeStructReference
{
    std::string Target{};
    std::string TypeName{};
    bool IsEmbedded{};
};

using ShaderUniformTypeVariant = std::variant<
    ShaderUniformTypeScalar,
    ShaderUniformTypeVector,
    ShaderUniformTypeMatrix,
    std::shared_ptr<ShaderUniformTypeArray>,
    std::shared_ptr<ShaderUniformTypeStruct>,
    ShaderUniformTypeStructReference>;

struct ShaderUniformType
{
    u32 SizeBytes{0};
    ShaderUniformTypeKind TypeKind{ShaderUniformTypeKind::None};
    ShaderUniformTypeVariant Type{};
};

struct ShaderUniformVariable
{
    std::string Name{};
    std::optional<std::string> DefaultValue{std::nullopt};
    u32 OffsetBytes{0};
    ShaderUniformType Type{};
};

struct ShaderUniformTypeArray
{
    u32 Size{1};
    ShaderUniformType Element{};
};

struct ShaderUniformTypeStruct
{
    std::string TypeName{};
    std::vector<ShaderUniformVariable> Fields{};
};

struct ShaderUniformTypeEmbeddedStruct
{
    ShaderUniformTypeStruct Struct{};
    AssetId Id{};
};

struct ShaderUniform
{
    ShaderUniformVariable Root{};
    std::vector<ShaderUniformTypeEmbeddedStruct> EmbeddedStructs{};
};

namespace shader
{
io::IoResult<std::string> packUniformStruct(const ShaderUniformTypeStruct& uniformStruct);
io::IoResult<ShaderUniformTypeStruct> unpackUniformStruct(const std::string& uniformStruct);

io::IoResult<std::string> packUniform(const ShaderUniform& uniform);
io::IoResult<ShaderUniform> unpackUniform(const std::string& uniform);
}
}