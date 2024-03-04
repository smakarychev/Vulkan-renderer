#include "SynchronizationTraits.h"

namespace SynchronizationUtils
{
    std::string pipelineStageToString(PipelineStage stage)
    {
        switch (stage)
        {
        case PipelineStage::None:                   return "None";
        case PipelineStage::Top:                    return "Top";
        case PipelineStage::Indirect:               return "Indirect";
        case PipelineStage::VertexInput:            return "VertexInput";
        case PipelineStage::IndexInput:             return "IndexInput";
        case PipelineStage::AttributeInput:         return "AttributeInput";
        case PipelineStage::VertexShader:           return "VertexShader";
        case PipelineStage::HullShader:             return "HullShader";
        case PipelineStage::DomainShader:           return "DomainShader";
        case PipelineStage::GeometryShader:         return "GeometryShader";
        case PipelineStage::PixelShader:            return "PixelShader";
        case PipelineStage::DepthEarly:             return "DepthEarly";
        case PipelineStage::DepthLate:              return "DepthLate";
        case PipelineStage::ColorOutput:            return "ColorOutput";
        case PipelineStage::ComputeShader:          return "ComputeShader";
        case PipelineStage::Copy:                   return "Copy";
        case PipelineStage::Blit:                   return "Blit";
        case PipelineStage::Resolve:                return "Resolve";
        case PipelineStage::Clear:                  return "Clear";
        case PipelineStage::AllTransfer:            return "AllTransfer";
        case PipelineStage::AllGraphics:            return "AllGraphics";
        case PipelineStage::AllPreRasterization:    return "AllPreRasterization";
        case PipelineStage::AllCommands:            return "AllCommands";
        case PipelineStage::Bottom:                 return "Bottom";
        case PipelineStage::Host:                   return "Host";
        case PipelineStage::TransformFeedback:      return "TransformFeedback";
        case PipelineStage::ConditionalRendering:   return "ConditionalRendering";
        default: return "";
        }
    }

    std::string pipelineAccessToString(PipelineAccess access)
    {
        switch (access)
        {
        case PipelineAccess::None:                          return "None";
        case PipelineAccess::ReadIndirect:                  return "ReadIndirect";
        case PipelineAccess::ReadIndex:                     return "ReadIndex";
        case PipelineAccess::ReadAttribute:                 return "ReadAttribute";
        case PipelineAccess::ReadUniform:                   return "ReadUniform";
        case PipelineAccess::ReadInputAttachment:           return "ReadInputAttachment";
        case PipelineAccess::ReadColorAttachment:           return "ReadColorAttachment";
        case PipelineAccess::ReadDepthStencilAttachment:    return "ReadDepthStencilAttachment";
        case PipelineAccess::ReadTransfer:                  return "ReadTransfer";
        case PipelineAccess::ReadHost:                      return "ReadHost";
        case PipelineAccess::ReadSampled:                   return "ReadSampled";
        case PipelineAccess::ReadStorage:                   return "ReadStorage";
        case PipelineAccess::ReadShader:                    return "ReadShader";
        case PipelineAccess::ReadAll:                       return "ReadAll";
        case PipelineAccess::WriteColorAttachment:          return "WriteColorAttachment";
        case PipelineAccess::WriteDepthStencilAttachment:   return "WriteDepthStencilAttachment";
        case PipelineAccess::WriteTransfer:                 return "WriteTransfer";
        case PipelineAccess::WriteHost:                     return "WriteHost";
        case PipelineAccess::WriteShader:                   return "WriteShader";
        case PipelineAccess::WriteAll:                      return "WriteAll";
        case PipelineAccess::ReadFeedbackCounter:           return "ReadFeedbackCounter";
        case PipelineAccess::WriteFeedbackCounter:          return "WriteFeedbackCounter";
        case PipelineAccess::WriteFeedback:                 return "WriteFeedback";
        case PipelineAccess::ReadConditional:               return "ReadConditional";
        default: return "";
        }
    }
}
