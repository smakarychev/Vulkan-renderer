#include "rendererpch.h"

#include "BufferTraits.h"

namespace BufferTraits
{
    std::string BufferTraits::bufferUsageToString(BufferUsage usage)
    {
        std::string usageString = "";

        if (enumHasAny(usage, BufferUsage::Vertex))
            usageString += usageString.empty() ? "Vertex" : " | Vertex";
        if (enumHasAny(usage, BufferUsage::Index))
            usageString += usageString.empty() ? "Index" : " | Index";
        if (enumHasAny(usage, BufferUsage::Uniform))
            usageString += usageString.empty() ? "Uniform" : " | Uniform";
        if (enumHasAny(usage, BufferUsage::Storage))
            usageString += usageString.empty() ? "Storage" : " | Storage";
        if (enumHasAny(usage, BufferUsage::Indirect))
            usageString += usageString.empty() ? "Indirect" : " | Indirect";
        if (enumHasAny(usage, BufferUsage::Mappable))
            usageString += usageString.empty() ? "Mappable" : " | Mappable";
        if (enumHasAny(usage, BufferUsage::MappableRandomAccess))
            usageString += usageString.empty() ? "MappableRandomAccess" : " | MappableRandomAccess";
        if (enumHasAny(usage, BufferUsage::Source))
            usageString += usageString.empty() ? "Source" : " | Source";
        if (enumHasAny(usage, BufferUsage::Destination))
            usageString += usageString.empty() ? "Destination" : " | Destination";
        if (enumHasAny(usage, BufferUsage::Conditional))
            usageString += usageString.empty() ? "Conditional" : " | Conditional";
        if (enumHasAny(usage, BufferUsage::DeviceAddress))
            usageString += usageString.empty() ? "DeviceAddress" : " | DeviceAddress";

        return usageString;
    }
}