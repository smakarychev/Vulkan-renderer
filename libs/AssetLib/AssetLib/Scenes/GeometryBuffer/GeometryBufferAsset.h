#pragma once
#include <AssetLib/Io/AssetIo.h>
#include <CoreLib/core.h>
#include <CoreLib/Math/Transform.h>

namespace lux::assetlib
{
namespace io
{
class AssetCompressor;
class AssetIoInterface;
}

static constexpr u32 GEOMETRY_UNSET_INDEX = ~0u; 

enum class GeometryBufferAccessorComponentType : u8
{
    U8, U16, U32, F32, Meshlet
};
enum class GeometryBufferAccessorType : u8
{
    Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
};
struct GeometryBufferAccessor
{
    struct SparseAccessor
    {
        struct IndicesAccessor
        {
            u32 BufferView{GEOMETRY_UNSET_INDEX};
            u64 OffsetBytes{0};
            GeometryBufferAccessorComponentType ComponentType{GeometryBufferAccessorComponentType::U8};
        };
        struct DataAccessor
        {
            u32 BufferView{GEOMETRY_UNSET_INDEX};
            u64 OffsetBytes{0};
        };
        
        u32 Count{0};
        IndicesAccessor Indices{};
        DataAccessor Data{};
    };
    u32 BufferView{GEOMETRY_UNSET_INDEX};
    u64 OffsetBytes{0};
    GeometryBufferAccessorComponentType ComponentType{GeometryBufferAccessorComponentType::U8};
    u32 Count{0};
    GeometryBufferAccessorType Type{GeometryBufferAccessorType::Scalar};
    bool Normalize{false};
    std::optional<SparseAccessor> Sparse{};
};
enum class GeometryBufferViewType : u8
{
    /* vertex attributes */
    Position = 0,
    Normal = 1,
    Tangent = 2,
    Uv = 3,
    Joint = 4,
    Weight = 5,
            
    /* index */
    Index = 6,

    /* per instance data */
    Meshlet = 7,
    
    InverseBindMatrix = 8,
    
    AnimationTimestamp = 9,
    AnimationPositionKeyframe = 10,
    AnimationOrientationKeyframe = 11,
    AnimationScaleKeyframe = 12,
    AnimationWeightKeyframe = 13,
    
    SparseAccessorIndex = 14,
            
    MaxVal = 15,
};
struct GeometryBufferView
{
    std::string Name;
    u32 Buffer{GEOMETRY_UNSET_INDEX};
    u64 OffsetBytes{0};
    u64 LengthBytes{0};
};

struct GeometryBufferHeader
{
    u64 SizeBytes{};
    std::vector<GeometryBufferAccessor> Accessors;
    std::vector<GeometryBufferView> BufferViews;
};

struct GeometryBufferAsset
{
    GeometryBufferHeader Header{};
    std::vector<std::byte> Data{};
};

namespace sceneGeometry
{
io::IoResult<GeometryBufferHeader> readHeader(const AssetMetadata& metadata);
io::IoResult<std::vector<std::byte>> readBufferData(const GeometryBufferHeader& header, const AssetMetadata& metadata,
    io::AssetIoInterface& io, io::AssetCompressor& compressor);

io::IoResult<AssetPacked> pack(const GeometryBufferAsset& geometry, io::AssetCompressor& compressor);
}
}