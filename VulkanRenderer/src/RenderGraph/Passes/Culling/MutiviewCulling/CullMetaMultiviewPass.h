#pragma once

#include "RenderGraph/RenderGraph.h"

class CullMultiviewData;

struct CullMetaMultiviewPassInitInfo
{
    const CullMultiviewData* MultiviewData{nullptr};
};

struct CullMetaMultiviewPassExecutionInfo
{
};

class CullMetaMultiviewPass
{
public:
    CullMetaMultiviewPass(RG::Graph& renderGraph, std::string_view name, const CullMetaMultiviewPassInitInfo& info);
private:
    RG::PassName m_Name;

    const CullMultiviewData* m_MultiviewData{nullptr};
};
