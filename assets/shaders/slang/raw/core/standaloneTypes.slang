module standaloneTypes;

import "core/viewInfo";
import "core/geometry";

struct Resources {
    ViewInfo view;
    RenderObject ro;
    IndirectCommand ic;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}