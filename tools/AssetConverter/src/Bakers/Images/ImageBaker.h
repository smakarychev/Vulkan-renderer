#pragma once

#include "Bakers/BakerContext.h"
#include "Bakers/Bakers.h"
#include "v2/Images/ImageAsset.h"
#include "v2/Images/ImageLoadInfo.h"

namespace lux::bakers
{
enum class ImageBakeNameHeuristics : u8
{
    None, Gltf,
};
struct ImageBakeSettings
{
    /* If `.meta` file does not exist for to-be-baked image, it will be generated.
     * `BakedFormat` and `NameHeuristics` are used to determine the baked image format
     * If `NameHeuristics` and `BakedFormat` are both not set, the default srgb format will be chosen
     * if `NameHeuristics` is Gltf, the image format will be chosen in accordance with gltf spec.
     * `BakedFormat` takes priority over `NameHeuristics`
     */
    assetlib::ImageFormat BakedFormat{assetlib::ImageFormat::Undefined};
    ImageBakeNameHeuristics NameHeuristics{ImageBakeNameHeuristics::Gltf};
};

class ImageBaker
{
public:
    static constexpr std::string_view IMAGE_LOAD_INFO_EXTENSION = ".meta";
public:
    static std::filesystem::path GetBakedPath(const std::filesystem::path& originalFile,
        const ImageBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::ImageAsset> BakeToFile(const std::filesystem::path& path,
        const ImageBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::ImageAsset> Bake(const assetlib::ImageLoadInfo& loadInfo,
        const ImageBakeSettings& settings, const Context& ctx);

    bool ShouldBake(const std::filesystem::path& path, const ImageBakeSettings& settings, const Context& ctx);
private:
    IoResult<assetlib::ImageAsset> BakeHDR(const assetlib::ImageLoadInfo& loadInfo);
    IoResult<assetlib::ImageAsset> BakeLDRKtx(const assetlib::ImageLoadInfo& loadInfo);
    IoResult<assetlib::ImageAsset> BakeLDRJpg(const assetlib::ImageLoadInfo& loadInfo);
private:
    // todo: rename to .tx once ready
    static constexpr std::string_view POST_BAKE_EXTENSION = IMAGE_ASSET_EXTENSION;
};
}
