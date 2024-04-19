#pragma once

#include "MeshCullPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class MeshCullContext;

class MeshletCullTranslucentContext
{
public:
    struct PassResources
    {
        RG::Resource MeshletsSsbo{};
        RG::Resource CommandsSsbo{};
    };
public:
    MeshletCullTranslucentContext(MeshCullContext& meshCullContext);

    const RG::Geometry& Geometry() { return m_MeshCullContext->Geometry(); }
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
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    MeshletCullTranslucentPass(RG::Graph& renderGraph, std::string_view name, CameraType cameraType);
    void AddToGraph(RG::Graph& renderGraph, MeshletCullTranslucentContext& ctx);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};