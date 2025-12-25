module standaloneTypes;

import "core/viewInfo";
import "core/geometry";
import "light/light";
import "materials/material";
import "shadow/shadow";

struct Resources {
    ViewInfo view;
    RenderObject ro;
    IndirectCommand ic;
    PointLight pl;
    DirectionalLight dl;
    StandardPbrMaterial standardPbrMaterial;
    CsmData csm;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}