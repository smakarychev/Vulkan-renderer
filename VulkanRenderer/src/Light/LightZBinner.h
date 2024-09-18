#pragma once

#include <vector>

#include "Settings.h"
#include "types.h"

class Camera;
class SceneLight;

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
    static ZBins ZBinLights(SceneLight& light, const Camera& camera);
};
