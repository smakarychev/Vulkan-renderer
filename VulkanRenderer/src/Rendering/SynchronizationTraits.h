#pragma once

#include "Core/core.h"

enum class PipelineStage
{
    None = 0,
    
    Top                     = BIT(1),
    Indirect                = BIT(2),
    VertexInput             = BIT(3),
    IndexInput              = BIT(4),
    AttributeInput          = BIT(5),
    VertexShader            = BIT(6),
    HullShader              = BIT(7),
    DomainShader            = BIT(8),
    GeometryShader          = BIT(9),
    PixelShader             = BIT(10),
    DepthEarly              = BIT(11),
    DepthLate               = BIT(12),
    ColorOutput             = BIT(13),
    ComputeShader           = BIT(14),
    Copy                    = BIT(15),
    Blit                    = BIT(16),
    Resolve                 = BIT(17),
    Clear                   = BIT(18),
    AllTransfer             = BIT(19),
    AllGraphics             = BIT(20),
    AllPreRasterization     = BIT(21),
    AllCommands             = BIT(22),
    Bottom                  = BIT(23),
    Host                    = BIT(24),
    TransformFeedback       = BIT(25),
    ConditionalRendering    = BIT(26),
};

CREATE_ENUM_FLAGS_OPERATORS(PipelineStage)

enum class PipelineAccess
{
    None = 0,

    ReadIndirect                    = BIT(1),
    ReadIndex                       = BIT(2),
    ReadAttribute                   = BIT(3),
    ReadUniform                     = BIT(4),
    ReadInputAttachment             = BIT(5),
    ReadColorAttachment             = BIT(6),
    ReadDepthStencilAttachment      = BIT(7),
    ReadTransfer                    = BIT(8),
    ReadHost                        = BIT(9),
    ReadSampled                     = BIT(10),
    ReadStorage                     = BIT(11),
    ReadShader                      = BIT(12),
    ReadAll                         = BIT(13),
    
    WriteColorAttachment            = BIT(14),
    WriteDepthStencilAttachment     = BIT(15),
    WriteTransfer                   = BIT(16),
    WriteHost                       = BIT(17),
    WriteShader                     = BIT(18),
    WriteAll                        = BIT(19),
    
    ReadFeedbackCounter             = BIT(20),
    WriteFeedbackCounter            = BIT(21),
    WriteFeedback                   = BIT(22),
    
    ReadConditional                 = BIT(22)
};

CREATE_ENUM_FLAGS_OPERATORS(PipelineAccess)

enum class PipelineDependencyFlags
{
    None = 0,

    ByRegion        = BIT(1),
    DeviceGroup     = BIT(2),    
    LocalView       = BIT(3),
    FeedbackLoop    = BIT(4)
};

CREATE_ENUM_FLAGS_OPERATORS(PipelineDependencyFlags)
