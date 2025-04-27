#pragma once

#include "Scene/ScenePass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"

#include "Scene/Visibility/SceneVisibility.h"

#include <functional>

struct SceneView;

struct SceneDrawPassExecutionInfo
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    RG::DrawAttachments Attachments{};
    ShaderOverrides* BucketOverrides{nullptr};
};
struct SceneDrawPassResources
{
    RG::Resource Draws{};
    RG::Resource DrawInfo{};
    RG::Resource Camera{};
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
    SceneView View{};
    SceneVisibilityHandle Visibility{};
    RG::DrawAttachments Attachments{};
};

class SceneDrawPassViewAttachments
{
public:
    const RG::DrawAttachments& Get(StringId viewName, StringId passName) const;
    RG::DrawAttachments& Get(StringId viewName, StringId passName);

    void Add(StringId viewName, StringId passName, const RG::DrawAttachments& attachments);
    // todo: remove this once RenderGraph is fixed!
    void UpdateResources(const RG::DrawAttachments& old, const RG::DrawAttachmentResources& resources);
private:
    using PassNameToAttachments = std::unordered_map<StringId, RG::DrawAttachments>;
    std::unordered_map<StringId, PassNameToAttachments> m_Attachments;
};