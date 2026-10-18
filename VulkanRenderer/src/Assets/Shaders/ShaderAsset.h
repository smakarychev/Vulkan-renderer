#pragma once

#include "Assets/AssetHandle.h"
#include "Rendering/Shader/ShaderPipelineTemplate.h"

namespace lux
{
struct ShaderAsset
{
    ShaderPipelineTemplate PipelineTemplate{};
    bool HasTextureHeap{false};
};

using ShaderHandle = AssetHandle<ShaderAsset>;
}
