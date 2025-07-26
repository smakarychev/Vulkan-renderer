#pragma once

#include "types.h"

namespace Passes::Clouds
{
    struct CloudsNoiseParameters
    {
        f32 PerlinCoverageMin{0.65f};
        f32 PerlinCoverageMax{1.7f};
        f32 WorleyCoverageMin{0.2f};
        f32 WorleyCoverageMax{1.0f};
        f32 PerlinWorleyFraction{0.88f};
        f32 NoiseDensityBias{0.4f};
    };

    static constexpr f32 REPROJECTION_RELATIVE_SIZE = 0.25f;
    static constexpr f32 REPROJECTION_RELATIVE_SIZE_INV = 1.0f / REPROJECTION_RELATIVE_SIZE;
}
