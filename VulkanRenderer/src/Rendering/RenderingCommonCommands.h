#pragma once
#include "RenderObject.h"

// todo: this file is temp until i do not have proper commands
struct IndirectDrawCommand
{
    u32 IndexCount;
    u32 InstanceCount;
    u32 FirstIndex;
    i32 VertexOffset;
    u32 FirstInstance;
    RenderHandle<RenderObject> RenderObject;
};

struct IndirectDispatchCommand
{
    u32 GroupX;
    u32 GroupY;
    u32 GroupZ;
};