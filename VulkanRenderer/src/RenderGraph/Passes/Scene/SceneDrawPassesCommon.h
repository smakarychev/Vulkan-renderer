#pragma once

#include "Scene/ScenePass.h"

#include <functional>

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"

class SceneBucket;

struct SceneBucketPassExecutionInfoCommon
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    RG::DrawAttachments Attachments{};
};
struct SceneBucketPassInitOutput
{
    const RG::Pass* Pass{nullptr};
    RG::DrawAttachmentResources Attachments{};
};
using SceneBucketPassInit =
    std::function<SceneBucketPassInitOutput(StringId name, RG::Graph&, const SceneBucketPassExecutionInfoCommon&)>;

struct SceneBucketPassInfo
{
    const SceneBucket* Bucket{nullptr};
    SceneBucketPassInit DrawPassInit{};
};

struct SceneViewDrawPassInfo
{
    const SceneView* View{nullptr};
    std::vector<SceneBucketPassInfo> BucketsPasses{};
    RG::DrawAttachments Attachments{};
};
