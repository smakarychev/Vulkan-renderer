module standaloneTypes;

import "core/viewInfo";

struct Resources {
    ViewInfo view;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}