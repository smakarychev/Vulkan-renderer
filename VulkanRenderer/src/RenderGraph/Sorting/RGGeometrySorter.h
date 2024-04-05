#pragma once

namespace RG
{
    class Geometry;
    
    class RGGeometrySorter
    {
    public:
        virtual ~RGGeometrySorter() = default;
        virtual void Sort(RG::Geometry* geometry) = 0; 
    };
}

