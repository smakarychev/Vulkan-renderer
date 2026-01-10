module bindlessMaterials;

import "material";

struct Dummy {
    ConstantBuffer<uint> dummy;
}

ParameterBlock<Dummy> d0;
ParameterBlock<Dummy> d1;
ParameterBlock<StandardPbrMaterials> materials;

[shader("vertex")]
void vertexMain() {
    float p = materials.materials[0].albedo.r;
    float p2 = materials.textures[0][int2(0)].r;
    unused(p);
    unused(p2);
}

[shader("pixel")]
void pixelMain() {
    float p = materials.materials[0].albedo.r;
    float p2 = materials.textures[0][int2(0)].r;
    unused(p);
    unused(p2);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain() {
    float p = materials.materials[0].albedo.r;
    float p2 = materials.textures[0][int2(0)].r;
    unused(p);
    unused(p2);
}