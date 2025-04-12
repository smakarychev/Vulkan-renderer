#pragma once

#include <functional>

#include "RenderGraph/RGResource.h"

class SceneBucket;

enum class SceneIndexBufferType
{
    U8, U16, U32
};

struct SceneBaseDrawPassData
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    RG::Resource IndexBuffer{};
    SceneIndexBufferType IndexBufferType{SceneIndexBufferType::U8};
};

struct SceneDrawPassInfo
{
    RG::Pass* Pass{nullptr};
    SceneBaseDrawPassData* DrawPassData{nullptr};
};

using InitSceneDrawPass = std::function<SceneDrawPassInfo(StringId parent)>;

struct SceneBucketDrawPassInfo
{
    SceneBucket* Bucket{nullptr};
    InitSceneDrawPass PassInit{};    
};


