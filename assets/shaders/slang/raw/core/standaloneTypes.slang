module standaloneTypes;

import "core/viewInfo";
import "core/geometry";
import "light/light";
import "light/culling/lightCulling";
import "materials/material";
import "shadow/shadow";
import "skinning/skinning";
import "volumetrics/volumetrics";
import "scene/visibility/visibility";
import "pbr/cameraExposure";

struct Resources {
    ViewInfo view;
    ShadingSettings shs;
    AtmosphereSettings as;
    SceneVisibilityCountData svcd;
    RenderObject ro;
    RenderObjectSkinnedInfo rosi;
    Skin sk;
    BlendShape bs;
    Meshlet mshlt;
    MeshletBounds mshltbnds;
    DrawInfo di;
    MeshletBucketInfo mbi;
    SceneVisibilityElement sve;
    IndirectCommand ic;
    SH2Irradiance sh2i;
    PointLight pl;
    DirectionalLight dl;
    LightCluster lc;
    LightTile lt;
    Plane p;
    StandardPbrMaterial standardPbrMaterial;
    CsmData csm;
    CloudsVPParameters cvp;
    CloudsNoiseParameters cnp;
    SkinDispatchInfo sdi;
    SkinnedVertex sv;
    ExposureFromHistogramOutput efho;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}