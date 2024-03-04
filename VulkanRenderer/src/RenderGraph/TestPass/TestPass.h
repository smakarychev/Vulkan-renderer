#pragma once
#include "RenderGraph/RenderGraph.h"

class TestPass
{
public:
    TestPass(RenderGraph::Graph& graph, const Texture& colorTarget);
private:
    struct TestPassData
    {
        RenderGraph::Resource ColorUbo{};
        RenderGraph::Resource ColorTarget{};
        glm::vec3 Color;
    };
    struct TestPassPostData
    {
        RenderGraph::Resource ColorIn;
        RenderGraph::Resource ColorTarget{};
        RenderGraph::Resource TimeUbo{};
    };
    RenderGraph::Pass* m_TestPass{nullptr};
    RenderGraph::Pass* m_TestPassPost{nullptr};
};
