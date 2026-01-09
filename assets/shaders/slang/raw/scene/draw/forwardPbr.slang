module forwardPbr;

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/viewInfo";
import "core/ugb";
import "pbr/pbr";
import "shadow/shadow";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
    [ImmutableSampler(SamplerFlags.Shadow | SamplerFlags.Linear)]
    SamplerComparisonState shadowSampler;
    [ImmutableSampler(SamplerFlags.ClampBlack | SamplerFlags.Linear)]
    SamplerState blackSampler;
    PbrShadingSamplers pbr;
}

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<IndirectCommand> commands;
    CsmResources csm;
    Texture2D<float> ssao;
    PbrShadingResources pbr;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    nointerpolation uint materialId;
    float3 positionWs;
    float3 normal;
    float4 tangent;
    float2 uv;
    float viewZ;
};

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;
ParameterBlock<StandardPbrMaterials> materials;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const IndirectCommand command = resources.commands[baseInstance];
    const RenderObject renderObject = resources.renderObjects[command.renderObject];
    const float4x4 transform = renderObject.transform;
    const uint vertexOffset = command.vertexOffset + vertexID;
    const float3 position = resources.ugb.position(renderObject.positionIndex + vertexOffset);

    const float4 positionWs = mul(float4(position, 1.0f), transform);

    VSOutput output;
    output.materialId = renderObject.materialId;
    output.position = mul(positionWs, resources.view.camera.viewProjection);
    output.positionWs = positionWs.xyz;
    output.normal = mul(resources.ugb.normal(renderObject.normalIndex + vertexOffset), float3x3(renderObject.transform));
    output.tangent = resources.ugb.tangent(renderObject.tangentIndex + vertexOffset);
    output.tangent.xyz = mul(output.tangent.xyz, float3x3(renderObject.transform));
    output.uv = resources.ugb.uv(renderObject.uvIndex + vertexOffset);
    output.viewZ = dot(positionWs, resources.view.camera.view._m02_m12_m22_m32);

    return output;
}

[shader("pixel")]
float4 pixelMain(VSOutput input) {
    const StandardPbrMaterial material = materials.materials[input.materialId];

    const float4 albedo = material.albedo * 
        materials.textures[NonUniformResourceIndex(material.albedoTexture)].Sample(samplers.sampler, input.uv);
#if ALPHA_TEST
    if (albedo.a < 0.5f)
        discard;
#endif // ALPHA_TEST

    const float2 screenUv = input.position.xy / resources.view.camera.resolution;

    const float3 emissive = materials.textures[NonUniformResourceIndex(material.emissiveTexture)].Sample(
        samplers.sampler, input.uv).rgb;

    float3 normal = normalize(input.normal);
    const float3 flatNormal = normal;
    float3 tangent = normalize(input.tangent.xyz);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    const float3 bitangent = normalize(cross(normal, tangent) * input.tangent.w);
    float3 normalMap = 
        materials.textures[NonUniformResourceIndex(material.normalTexture)].Sample(samplers.sampler, input.uv).rgb;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    normal = tangent * normalMap.x + bitangent * normalMap.y + normal * normalMap.z;

    const float2 metallicRoughness = 
        materials.textures[NonUniformResourceIndex(material.metallicRoughnessTexture)].Sample(
            samplers.sampler, input.uv).rg;
    const float metallic = metallicRoughness.r * material.metallic;
    const float roughness = metallicRoughness.g * material.roughness;
    
    PcfShadowSampler pcfShadowSampler;
    CsmShadow csmShadow = CsmShadow(resources.csm.csmData, resources.csm.csmTexture, samplers.shadowSampler,
        resources.view.shading.primaryDirectionalLightDirection);
    float shadow = 1.0f - csmShadow.sample(pcfShadowSampler, input.positionWs, flatNormal, input.viewZ);
    CloudsShadow cloudsShadow = CloudsShadow(
        resources.view, materials.textures[resources.view.shading.volumetricCloudShadow], samplers.blackSampler);
    shadow *= 1.0f - cloudsShadow.sample(input.positionWs);

    PbrShadingInfo shadingInfo;
    shadingInfo.position = input.positionWs;
    shadingInfo.normal = normal;
    shadingInfo.viewZ = input.viewZ;
    shadingInfo.depth = input.position.z;
    shadingInfo.view = normalize(resources.view.camera.position - input.positionWs);
    shadingInfo.NoV = clamp(dot(shadingInfo.normal, shadingInfo.view), 1e-5f, 1.0f);
    shadingInfo.roughness = roughness * roughness;
    shadingInfo.metallic = metallic;
    shadingInfo.F0 = PbrShadingInfo.calculateF0(albedo, metallic);
    shadingInfo.F90 = PbrShadingInfo.calculateF90();
    shadingInfo.diffuseColor = 
        PbrShadingInfo.calculateDiffuseColor(albedo, metallic, shadingInfo.F0);
    shadingInfo.specularColor = PbrShadingInfo.calculateSpecularColor(shadingInfo.F0);
    shadingInfo.screenUv = screenUv;
    shadingInfo.directionalShadow = shadow;

    const float ao = resources.ssao.SampleLevel(samplers.sampler, screenUv, 0) *
        materials.textures[NonUniformResourceIndex(material.ambientOcclusionTexture)].Sample(
            samplers.sampler, input.uv).r;

    return float4(pbrShade(resources.view, shadingInfo, samplers.pbr, resources.pbr) + emissive, 1.0f);
}