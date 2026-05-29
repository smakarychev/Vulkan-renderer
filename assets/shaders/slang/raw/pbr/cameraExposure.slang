module cameraExposure;

import "core/viewInfo";
import "pbr/pbr";

struct Resources {
    RWStructuredBuffer<ViewInfo> view;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(
    uniform float aperture,
    uniform float shutterTime,
    uniform float ISO) {

    resources.view[0].shading.exposure = convertEV100ToExposure(computeEV100(aperture, shutterTime, ISO));
}