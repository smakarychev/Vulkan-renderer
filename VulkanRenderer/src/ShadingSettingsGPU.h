#pragma once

#include "types.h"

/*
 * Used for most of the 'pbr' passes of render graph
 * Reflected in `pbr/common.glsl`
 */
struct ShadingSettingsGPU
{
    f32 EnvironmentPower{1.0f};
    u32 SoftShadows{false};  
};