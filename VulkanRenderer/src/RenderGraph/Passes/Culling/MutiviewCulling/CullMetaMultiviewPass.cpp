#include "CullMetaMultiviewPass.h"

#include "CullMultiviewData.h"

CullMetaMultiviewPass::CullMetaMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const CullMetaMultiviewPassInitInfo& info)
        : m_Name(name), m_MultiviewData(info.MultiviewData)
{
    
}
