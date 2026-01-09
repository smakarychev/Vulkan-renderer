module standaloneTypes;

import "core/viewInfo";
import "core/geometry";
import "light/light";
import "materials/material";
import "shadow/shadow";
import "volumetrics/volumetrics";

struct Resources {
    ViewInfo view;
    RenderObject ro;
    Meshlet mshlt;
    DrawInfo di;
    MeshletBucketInfo mbi;
    IndirectCommand ic;
    PointLight pl;
    DirectionalLight dl;
    StandardPbrMaterial standardPbrMaterial;
    CsmData csm;
    CloudsVPParameters cvp;
    CloudsNoiseParameters cnp;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}