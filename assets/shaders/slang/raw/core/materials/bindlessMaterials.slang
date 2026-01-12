module bindlessMaterials;

import "core/texturesHeap";

struct Dummy {
    ConstantBuffer<uint> dummy;
}

ParameterBlock<Dummy> d0;
ParameterBlock<Dummy> d1;
ParameterBlock<TextureHeap> textures;

[shader("vertex")]
void vertexMain() {
    float p = textures.textures[0][int2(0)].r;
    unused(p);
}

[shader("pixel")]
void pixelMain() {
    float p = textures.textures[0][int2(0)].r;
    unused(p);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain() {
    float p = textures.textures[0][int2(0)].r;
    unused(p);
}