#pragma once

#include "ViewInfoGPU.h"
#include "String/StringId.h"

struct SceneView
{
    StringId Name{};
    ViewInfoGPU ViewInfo{};
    auto operator==(const SceneView& other) const { return Name == other.Name; }
};