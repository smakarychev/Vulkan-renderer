#pragma once

#include "types.h"
#include "RenderGraph/Passes/Generated/Types/CloudsNoiseParametersUniform.generated.h"

namespace Passes::Clouds
{
struct CloudsNoiseParameters : gen::CloudsNoiseParameters
{
};

static constexpr f32 REPROJECTION_RELATIVE_SIZE = 0.25f;
static constexpr f32 REPROJECTION_RELATIVE_SIZE_INV = 1.0f / REPROJECTION_RELATIVE_SIZE;
}
