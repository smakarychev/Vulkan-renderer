#pragma once

class ResourceUploader;

class SceneGeometry;
    
class GeometrySorter
{
public:
    GeometrySorter() = default;
    virtual ~GeometrySorter() = default;
    virtual void Sort(SceneGeometry& geometry, ResourceUploader& resourceUploader) = 0; 
};
