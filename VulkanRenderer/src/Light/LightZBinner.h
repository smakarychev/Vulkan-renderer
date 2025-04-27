#pragma once

#include <vector>

#include "Settings.h"
#include "types.h"

class Camera;
class SceneLight2;

struct ZBins
{
    struct Bin
    {
        static constexpr u16 NO_LIGHT{std::numeric_limits<u16>::max()};
        u16 LightMin{NO_LIGHT};
        u16 LightMax{0};
    };
    std::vector<Bin> Bins{std::vector(LIGHT_TILE_BINS_Z, Bin{})};
};

class LightZBinner
{
public:
    // todo: this allocates 32 KiB every frame... maybe this is not the best way, although I doubt it matters at all
    // todo: maybe keep bins on a stack inside of std::array?
    static ZBins ZBinLights(SceneLight2& light, const Camera& camera);
};
