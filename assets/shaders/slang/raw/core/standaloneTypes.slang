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
    IndirectCommand ic;
    PointLight pl;
    DirectionalLight dl;
    StandardPbrMaterial standardPbrMaterial;
    CsmData csm;
    CloudsVPParameters cvp;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}