module vbufferPbr;

import "core/attributes";
import "core/geometry";
import "core/ugb";
import "core/materials/material";
import "core/viewInfo";

import "vbuffer";
import "utility/fullscreen";

import "pbr/pbr";
import "light/light";
import "light/culling/lightCulling";
import "shadow/shadow";

struct Samplers {
    VBufferSamplers vbufferSamplers;
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState linearSampler;
    [ImmutableSampler(SamplerFlags.Shadow | SamplerFlags.Linear)]
    SamplerComparisonState shadowSampler;
    [ImmutableSampler(SamplerFlags.ClampBlack | SamplerFlags.Linear)]
    SamplerState blackSampler;
    PbrShadingSamplers pbr;
}

struct Resources {
    VBufferResources vbufferResources;
    UGB ugb;
    ConstantBuffer<ViewInfo> view;
    StructuredBuffer<IndirectCommand> commands;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<uint8_t> indices;
    CsmResources csm;
    Texture2D<float> ssao;
    PbrShadingResources pbr;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;
ParameterBlock<StandardPbrMaterials> materials;

[shader("pixel")]
float4 main(FullScreenVSOutput input) {
    const VBufferVisibility visibility = VBufferVisibility(
        resources.vbufferResources.vbuffer.SampleLevel(samplers.vbufferSamplers.sampler, input.uv, 0));
    if (!visibility.isValid())
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    VBufferGeometryData gData = VBufferGeometryData(visibility, resources.ugb, resources.view, input.position,
                                                    resources.commands, resources.renderObjects, resources.indices);

    const StandardPbrMaterial material = materials.materials[gData.materialId];
    VBufferStandardPbrMaterialData materialData = VBufferStandardPbrMaterialData(gData, material, materials.textures,
        samplers.linearSampler);

    PcfShadowSampler pcfShadowSampler;
    CsmShadow csmShadow = CsmShadow(resources.csm.csmData, resources.csm.csmTexture, samplers.shadowSampler,
        resources.view.shading.primaryDirectionalLightDirection);
    float shadow = 1.0f - csmShadow.sample(pcfShadowSampler, gData.position, gData.normal, gData.viewZ);
    CloudsShadow cloudsShadow = CloudsShadow(
        resources.view, materials.textures[resources.view.shading.volumetricCloudShadow], samplers.blackSampler);
    shadow *= 1.0f - cloudsShadow.sample(gData.position);

    PbrShadingInfo shadingInfo;
    shadingInfo.position = gData.position;
    shadingInfo.normal = materialData.normal;
    shadingInfo.viewZ = gData.viewZ;
    shadingInfo.depth = gData.depth;
    shadingInfo.view = normalize(resources.view.camera.position - gData.position);
    shadingInfo.NoV = clamp(dot(shadingInfo.normal, shadingInfo.view), 1e-5f, 1.0f);
    shadingInfo.roughness = materialData.roughness * materialData.roughness;
    shadingInfo.metallic = materialData.metallic;
    shadingInfo.F0 = PbrShadingInfo.calculateF0(materialData.albedo, materialData.metallic);
    shadingInfo.F90 = PbrShadingInfo.calculateF90();
    shadingInfo.diffuseColor = 
        PbrShadingInfo.calculateDiffuseColor(materialData.albedo, materialData.metallic, shadingInfo.F0);
    shadingInfo.specularColor = PbrShadingInfo.calculateSpecularColor(shadingInfo.F0);
    shadingInfo.screenUv = input.uv;
    shadingInfo.directionalShadow = shadow;

    const float ao = resources.ssao.SampleLevel(samplers.linearSampler, input.uv, 0) * materialData.ao;

    return float4(pbrShade(resources.view, shadingInfo, samplers.pbr, resources.pbr), 1.0f);
}