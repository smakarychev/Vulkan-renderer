#pragma once

#include "types.h"

namespace Passes::Clouds
{
    struct CloudsNoiseParameters
    {
        f32 PerlinCoverageMin{0.52f};
        f32 PerlinCoverageMax{1.2f};
        f32 WorleyCoverageMin{0.225f};
        f32 WorleyCoverageMax{1.16f};
        f32 PerlinWorleyFraction{0.80f};
        f32 NoiseDensityBias{0.61f};
    };

    static constexpr f32 REPROJECTION_RELATIVE_SIZE = 0.25f;
    static constexpr f32 REPROJECTION_RELATIVE_SIZE_INV = 1.0f / REPROJECTION_RELATIVE_SIZE;
}
