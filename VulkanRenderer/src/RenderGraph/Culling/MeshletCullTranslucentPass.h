#pragma once

#include "MeshCullPass.h"
#include "RenderGraph/RenderGraph.h"
#include "..\RGCommon.h"
#include "..\RGGeometry.h"
#include "Rendering/Buffer.h"

class MeshCullContext;

class MeshletCullTranslucentContext
{
public:
    struct PassResources
    {
        RenderGraph::Resource MeshletsSsbo{};
        RenderGraph::Resource CommandsSsbo{};
    };
public:
    MeshletCullTranslucentContext(MeshCullContext& meshCullContext);

    const RenderPassGeometry& Geometry() { return m_MeshCullContext->Geometry(); }
    MeshCullContext& MeshContext() { return *m_MeshCullContext; }
    PassResources& Resources() { return m_Resources; }
private:
    MeshCullContext* m_MeshCullContext{nullptr};
    PassResources m_Resources{};
};

class MeshletCullTranslucentPass
{
public:
    struct PassData
    {
        MeshCullContext::PassResources MeshResources;
        MeshletCullTranslucentContext::PassResources MeshletResources;

        u32 MeshletCount;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    MeshletCullTranslucentPass(RenderGraph::Graph& renderGraph, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, MeshletCullTranslucentContext& ctx);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::PipelineData m_PipelineData;
};