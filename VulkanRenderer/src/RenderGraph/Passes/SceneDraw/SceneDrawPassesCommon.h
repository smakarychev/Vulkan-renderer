#pragma once

#include "Scene/ScenePass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGDrawResources.h"

#include "Scene/Visibility/SceneVisibility.h"

#include <functional>

struct SceneView;

struct SceneDrawPassExecutionInfo
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    glm::uvec2 Resolution{};
    RG::Resource ViewInfo{};
    RG::DrawAttachments Attachments{};
    ShaderOverrides* BucketOverrides{nullptr};
};
struct SceneDrawPassResources
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    RG::Resource ViewInfo{};
    RG::DrawAttachmentResources Attachments{};
    u32 MaxDrawCount{0};

    void CreateFrom(const SceneDrawPassExecutionInfo& info, RG::Graph& renderGraph);
};

using SceneDrawPassInitFn =
    std::function<RG::DrawAttachmentResources(StringId name, RG::Graph&, const SceneDrawPassExecutionInfo&)>;

struct SceneDrawPassDescription
{
    const ScenePass* Pass{nullptr};
    SceneDrawPassInitFn DrawPassInit{};    
    SceneView SceneView{};
    SceneVisibilityHandle Visibility{};
    RG::DrawAttachments Attachments{};
};

class SceneDrawPassViewAttachments
{
public:
    const RG::DrawAttachments& Get(StringId viewName, StringId passName) const;
    RG::DrawAttachments& Get(StringId viewName, StringId passName);
    RG::Resource GetMinMaxDepthReduction(StringId viewName) const;

    void Add(StringId viewName, StringId passName, const RG::DrawAttachments& attachments);
    void SetMinMaxDepthReduction(StringId viewName, RG::Resource minMaxDepthReduction);
private:
    using PassNameToAttachments = std::unordered_map<StringId, RG::DrawAttachments>;
    std::unordered_map<StringId, PassNameToAttachments> m_Attachments;
    std::unordered_map<StringId, RG::Resource> m_MinMaxDepthReductions;
};