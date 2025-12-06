module standaloneTypes;

import "core/viewInfo";
import "core/geometry";
import "shadow/shadow";

struct Resources {
    ViewInfo view;
    RenderObject ro;
    IndirectCommand ic;
    CsmData csm;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}