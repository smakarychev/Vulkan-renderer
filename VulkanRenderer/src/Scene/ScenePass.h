#pragma once

#include "Scene.h"

/* pass over filtered render objects of a scene */

class SceneBucket
{
public:
    using FilterFn = std::function<bool(const SceneGeometryInfo& geometry, u32 renderObjectIndex)>;

    SceneBucket(std::string_view name, FilterFn filter);
public:
    FilterFn Filter{};
private:
    std::string m_Name{};
};

class ScenePass
{
public:
    ScenePass(std::string_view name, Span<const SceneBucket> buckets);
    bool Filter(const SceneGeometryInfo& geometry, u32 renderObjectIndex);
private:
    std::vector<SceneBucket> m_Buckets;
    std::vector<u32> m_RenderObjectsBucketIndex;

    std::string m_Name{};
};
