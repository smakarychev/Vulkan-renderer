#pragma once

class ResourceUploader;

namespace RG
{
    class Geometry;
    
    class GeometrySorter
    {
    public:
        GeometrySorter() = default;
        virtual ~GeometrySorter() = default;
        virtual void Sort(Geometry& geometry, ResourceUploader& resourceUploader) = 0; 
    };
}

