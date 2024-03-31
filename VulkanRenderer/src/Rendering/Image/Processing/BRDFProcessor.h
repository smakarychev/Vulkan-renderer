#pragma once
#include "Rendering/Image/Image.h"

class BRDFProcessor
{
public:
    static Texture CreateBRDF(const CommandBuffer& cmd);
};
