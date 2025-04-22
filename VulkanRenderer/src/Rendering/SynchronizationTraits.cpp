#include "SynchronizationTraits.h"

namespace SynchronizationTraits
{
    std::string pipelineStageToString(PipelineStage stage)
    {
        std::string stageString;
        if (enumHasAny(stage, PipelineStage::Top))
            stageString += stageString.empty() ? "Top" : " | Top";
        if (enumHasAny(stage, PipelineStage::Indirect))
            stageString += stageString.empty() ? "Indirect" : " | Indirect";
        if (enumHasAny(stage, PipelineStage::VertexInput))
            stageString += stageString.empty() ? "VertexInput" : " | VertexInput";
        if (enumHasAny(stage, PipelineStage::IndexInput))
            stageString += stageString.empty() ? "IndexInput" : " | IndexInput";
        if (enumHasAny(stage, PipelineStage::AttributeInput))
            stageString += stageString.empty() ? "AttributeInput" : " | AttributeInput";
        if (enumHasAny(stage, PipelineStage::VertexShader))
            stageString += stageString.empty() ? "VertexShader" : " | VertexShader";
        if (enumHasAny(stage, PipelineStage::HullShader))
            stageString += stageString.empty() ? "HullShader" : " | HullShader";
        if (enumHasAny(stage, PipelineStage::DomainShader))
            stageString += stageString.empty() ? "DomainShader" : " | DomainShader";
        if (enumHasAny(stage, PipelineStage::GeometryShader))
            stageString += stageString.empty() ? "GeometryShader" : " | GeometryShader";
        if (enumHasAny(stage, PipelineStage::PixelShader))
            stageString += stageString.empty() ? "PixelShader" : " | PixelShader";
        if (enumHasAny(stage, PipelineStage::DepthEarly))
            stageString += stageString.empty() ? "DepthEarly" : " | DepthEarly";
        if (enumHasAny(stage, PipelineStage::DepthLate))
            stageString += stageString.empty() ? "DepthLate" : " | DepthLate";
        if (enumHasAny(stage, PipelineStage::ColorOutput))
            stageString += stageString.empty() ? "ColorOutput" : " | ColorOutput";
        if (enumHasAny(stage, PipelineStage::ComputeShader))
            stageString += stageString.empty() ? "ComputeShader" : " | ComputeShader";
        if (enumHasAny(stage, PipelineStage::Copy))
            stageString += stageString.empty() ? "Copy" : " | Copy";
        if (enumHasAny(stage, PipelineStage::Blit))
            stageString += stageString.empty() ? "Blit" : " | Blit";
        if (enumHasAny(stage, PipelineStage::Resolve))
            stageString += stageString.empty() ? "Resolve" : " | Resolve";
        if (enumHasAny(stage, PipelineStage::Clear))
            stageString += stageString.empty() ? "Clear" : " | Clear";
        if (enumHasAny(stage, PipelineStage::AllTransfer))
            stageString += stageString.empty() ? "AllTransfer" : " | AllTransfer";
        if (enumHasAny(stage, PipelineStage::AllGraphics))
            stageString += stageString.empty() ? "AllGraphics" : " | AllGraphics";
        if (enumHasAny(stage, PipelineStage::AllPreRasterization))
            stageString += stageString.empty() ? "AllPreRasterization" : " | AllPreRasterization";
        if (enumHasAny(stage, PipelineStage::AllCommands))
            stageString += stageString.empty() ? "AllCommands" : " | AllCommands";
        if (enumHasAny(stage, PipelineStage::Bottom))
            stageString += stageString.empty() ? "Bottom" : " | Bottom";
        if (enumHasAny(stage, PipelineStage::Host))
            stageString += stageString.empty() ? "Host" : " | Host";
        if (enumHasAny(stage, PipelineStage::TransformFeedback))
            stageString += stageString.empty() ? "TransformFeedback" : " | TransformFeedback";
        if (enumHasAny(stage, PipelineStage::ConditionalRendering))
            stageString += stageString.empty() ? "ConditionalRendering" : " | ConditionalRendering";

        return stageString;
    }

    std::string pipelineAccessToString(PipelineAccess access)
    {
        std::string accessString;
        if (enumHasAny(access, PipelineAccess::ReadIndirect))
            accessString += accessString.empty() ? "ReadIndirect" : " | ReadIndirect";
        if (enumHasAny(access, PipelineAccess::ReadIndex))
            accessString += accessString.empty() ? "ReadIndex" : " | ReadIndex";
        if (enumHasAny(access, PipelineAccess::ReadAttribute))
            accessString += accessString.empty() ? "ReadAttribute" : " | ReadAttribute";
        if (enumHasAny(access, PipelineAccess::ReadUniform))
            accessString += accessString.empty() ? "ReadUniform" : " | ReadUniform";
        if (enumHasAny(access, PipelineAccess::ReadInputAttachment))
            accessString += accessString.empty() ? "ReadInputAttachment" : " | ReadInputAttachment";
        if (enumHasAny(access, PipelineAccess::ReadColorAttachment))
            accessString += accessString.empty() ? "ReadColorAttachment" : " | ReadColorAttachment";
        if (enumHasAny(access, PipelineAccess::ReadDepthStencilAttachment))
            accessString += accessString.empty() ? "ReadDepthStencilAttachment" : " | ReadDepthStencilAttachment";
        if (enumHasAny(access, PipelineAccess::ReadTransfer))
            accessString += accessString.empty() ? "ReadTransfer" : " | ReadTransfer";
        if (enumHasAny(access, PipelineAccess::ReadHost))
            accessString += accessString.empty() ? "ReadHost" : " | ReadHost";
        if (enumHasAny(access, PipelineAccess::ReadSampled))
            accessString += accessString.empty() ? "ReadSampled" : " | ReadSampled";
        if (enumHasAny(access, PipelineAccess::ReadStorage))
            accessString += accessString.empty() ? "ReadStorage" : " | ReadStorage";
        if (enumHasAny(access, PipelineAccess::ReadShader))
            accessString += accessString.empty() ? "ReadShader" : " | ReadShader";
        if (enumHasAny(access, PipelineAccess::ReadAll))
            accessString += accessString.empty() ? "ReadAll" : " | ReadAll";
        if (enumHasAny(access, PipelineAccess::WriteColorAttachment))
            accessString += accessString.empty() ? "WriteColorAttachment" : " | WriteColorAttachment";
        if (enumHasAny(access, PipelineAccess::WriteDepthStencilAttachment))
            accessString += accessString.empty() ? "WriteDepthStencilAttachment" : " | WriteDepthStencilAttachment";
        if (enumHasAny(access, PipelineAccess::WriteTransfer))
            accessString += accessString.empty() ? "WriteTransfer" : " | WriteTransfer";
        if (enumHasAny(access, PipelineAccess::WriteHost))
            accessString += accessString.empty() ? "WriteHost" : " | WriteHost";
        if (enumHasAny(access, PipelineAccess::WriteShader))
            accessString += accessString.empty() ? "WriteShader" : " | WriteShader";
        if (enumHasAny(access, PipelineAccess::WriteAll))
            accessString += accessString.empty() ? "WriteAll" : " | WriteAll";
        if (enumHasAny(access, PipelineAccess::ReadFeedbackCounter))
            accessString += accessString.empty() ? "ReadFeedbackCounter" : " | ReadFeedbackCounter";
        if (enumHasAny(access, PipelineAccess::WriteFeedbackCounter))
            accessString += accessString.empty() ? "WriteFeedbackCounter" : " | WriteFeedbackCounter";
        if (enumHasAny(access, PipelineAccess::WriteFeedback))
            accessString += accessString.empty() ? "WriteFeedback" : " | WriteFeedback";
        if (enumHasAny(access, PipelineAccess::ReadConditional))
            accessString += accessString.empty() ? "ReadConditional" : " | ReadConditional";

        return accessString;
    }
}
