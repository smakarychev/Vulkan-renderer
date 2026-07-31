module momentBasedOITTransmittance;

// https://momentsingraphics.de/I3D2018.html

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/texturesHeap";
import "core/viewInfo";
import "core/ugb";

import "pbr/pbr";
import "shadow/shadow";
import "scene/visibility/visibility";
import "skinning/skinning";
import "translucentDepthBounds";

struct ForwardPassSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
    [ImmutableSampler(SamplerFlags.Shadow | SamplerFlags.Linear)]
    SamplerComparisonState shadowSampler;
    [ImmutableSampler(SamplerFlags.ClampBlack | SamplerFlags.Linear)]
    SamplerState blackSampler;
    PbrShadingSamplers pbr;
}

struct ForwardPassResources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneDrawElement> visibleMeshlets;
    CsmResources csm;
    Texture2D<float> ssao;
    PbrShadingResources pbr;
    StructuredBuffer<StandardPbrMaterial> materials;
    ConstantBuffer<DepthRange> depthRange;
    Texture2D<float16_t> absorbanceTotal;
    Texture2D<float16_t4> absorbanceMoments;
}

struct VSOutput {
    float4 position : SV_POSITION;
    nointerpolation uint materialId;
    float3 positionWs;
    float3 normal;
    float4 tangent;
    float2 uv;
    float viewZ;
};

ParameterBlock<ForwardPassSamplers> samplers;
ParameterBlock<ForwardPassResources> resources;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const SceneDrawElement element = resources.visibleMeshlets[baseInstance];
    const RenderObject renderObject = resources.renderObjects[element.renderObjectId];
    const Meshlet meshlet = resources.meshlets.meshlet(renderObject.meshletIndex + element.meshletId);
    const uint vertexOffset = meshlet.firstVertex + vertexID;
    float4x4 transform = renderObject.transform;

    float4 positionWs;
    float3 normal;
    float4 tangent;

    [branch]
    if (WaveActiveBitOr(renderObject.isDeformed())) {
        SkinnedVertex vertex = resources.ugb.skinnedVertex(renderObject.positionIndex + vertexOffset);
        positionWs = float4(vertex.position, 1.0f);
        normal = vertex.normal;
        tangent = vertex.tangent;
    }
    else {
        positionWs = mul(float4(resources.ugb.position(renderObject.positionIndex + vertexOffset), 1.0f), transform);
        normal = mul(resources.ugb.normal(renderObject.normalIndex + vertexOffset), float3x3(transform));
        tangent = resources.ugb.tangent(renderObject.tangentIndex + vertexOffset);
        tangent.xyz = mul(tangent.xyz, float3x3(transform));
    }

    VSOutput output;
    output.materialId = renderObject.materialId;
    output.position = mul(positionWs, resources.view.camera.viewProjection);
    output.positionWs = positionWs.xyz;
    output.normal = normal;
    output.tangent = tangent;
    output.uv = resources.ugb.uv(renderObject.uvIndex + vertexOffset);
    output.viewZ = dot(positionWs, resources.view.camera.view._m02_m12_m22_m32);

    return output;
}

[shader("pixel")]
[earlydepthstencil]
float4 pixelMain(VSOutput input, float2 coord : SV_Position) {
    const StandardPbrMaterial material = resources.materials[input.materialId];
    const float4 albedo = material.albedo * 
        nonuniform(material.albedoTexture.desc).Sample(samplers.sampler, input.uv);

    const float2 screenUv = input.position.xy / resources.view.camera.resolution;
    const float3 emissive = nonuniform(material.emissiveTexture.desc).Sample(samplers.sampler, input.uv).rgb;

    float3 normal = normalize(input.normal);
    const float3 flatNormal = normal;
    float3 tangent = normalize(input.tangent.xyz);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    const float3 bitangent = normalize(cross(normal, tangent) * input.tangent.w);
    float3 normalMap = 
        nonuniform(material.normalTexture.desc).Sample(samplers.sampler, input.uv).rgb;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    normal = tangent * normalMap.x + bitangent * normalMap.y + normal * normalMap.z;

    const float2 metallicRoughness = 
        nonuniform(material.metallicRoughnessTexture.desc).Sample(samplers.sampler, input.uv).rg;
    const float metallic = metallicRoughness.r * material.metallic;
    const float roughness = metallicRoughness.g * material.roughness;
    
    PcfShadowSampler pcfShadowSampler;
    CsmShadow csmShadow = CsmShadow(resources.csm.csmData, resources.csm.csmTexture, samplers.shadowSampler,
        resources.view.shading.primaryDirectionalLightDirection);
    float shadow = 1.0f - csmShadow.sample(pcfShadowSampler, input.positionWs, flatNormal);
    CloudsShadow cloudsShadow = CloudsShadow(
        resources.view, resources.view.shading.volumetricCloudShadow.desc, samplers.blackSampler);
    shadow *= 1.0f - cloudsShadow.sample(input.positionWs);

    PbrShadingInfo shadingInfo;
    shadingInfo.position = input.positionWs;
    shadingInfo.normal = normal;
    shadingInfo.viewZ = input.viewZ;
    shadingInfo.depth = input.position.z;
    shadingInfo.view = normalize(resources.view.camera.position - input.positionWs);
    shadingInfo.R = reflect(-shadingInfo.view, shadingInfo.normal);
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
    shadingInfo.ao = resources.ssao.SampleLevel(samplers.sampler, screenUv, 0) *
        nonuniform(material.ambientOcclusionTexture.desc).Sample(samplers.sampler, input.uv).r;

    const float b0 = resources.absorbanceTotal[int2(coord)];
	clip(b0 - 0.00100050033f);
    
    const float4 b = resources.absorbanceMoments[int2(coord)];
    const float2 bEvenQ = b.yw;
    const float2 bOddQ = b.xz;
    float2 bEven, bOdd;
    offsetAndDequantizeMoments(bEven, bOdd, bEvenQ, bOddQ);

    const float zMin = asfloat(resources.depthRange.min);
    const float zMax = asfloat(resources.depthRange.max);
    const float z = saturate((log(-input.viewZ) - log(zMin)) / (log(zMax) - log(zMin))) * 2.0f - 1.0f;
    const float transmittance = computeTransmittanceAtDepthFrom4PowerMoments(
        b0, bEven, bOdd, z, 3e-3f, 0.25f, float4(0.0f, 0.628f, 0.0f, 0.628f));
    const float opacity = transmittance * albedo.a;
    const float totalTransmittance = exp(-b0);

    return float4(
        opacity * (pbrShade(resources.view, shadingInfo, samplers.pbr, resources.pbr) + emissive), 
        opacity);
}

void offsetAndDequantizeMoments(out float2 bEven, out float2 bOdd, float2 bEvenQ, float2 bOddQ) {
    bOddQ -= 0.5f;

	bOdd = mul(bOddQ, float2x2(-1.0f / 3.0f, -0.75f, sqrt(3.0f), 0.75f*sqrt(3.0f)));
	bEven = mul(bEvenQ, float2x2(0.125f, -0.125f, 1.0f, 1.0f));
}

float computeTransmittanceAtDepthFrom4PowerMoments(float b0, float2 bEven, float2 bOdd, float depth, float bias, 
    float overestimation, float4 biasVector) {
	
    float4 b = float4(bOdd.x, bEven.x, bOdd.y, bEven.y);
	// Bias input data to avoid artifacts
	b = lerp(b, biasVector, bias);
	float3 z;
	z[0] = depth;

	// Compute a Cholesky factorization of the Hankel matrix B storing only non-
	// trivial entries or related products
	float L21D11=mad(-b[0],b[1],b[2]);
	float D11=mad(-b[0],b[0], b[1]);
	float InvD11=1.0f/D11;
	float L21=L21D11*InvD11;
	float SquaredDepthVariance=mad(-b[1],b[1], b[3]);
	float D22=mad(-L21D11,L21,SquaredDepthVariance);

	// Obtain a scaled inverse image of bz=(1,z[0],z[0]*z[0])^T
	float3 c=float3(1.0f,z[0],z[0]*z[0]);
	// Forward substitution to solve L*c1=bz
	c[1]-=b.x;
	c[2]-=b.y+L21*c[1];
	// Scaling to solve D*c2=c1
	c[1]*=InvD11;
	c[2]/=D22;
	// Backward substitution to solve L^T*c3=c2
	c[1]-=L21*c[2];
	c[0]-=dot(c.yz,b.xy);
	// Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions 
	// z[1] and z[2]
	float InvC2=1.0f/c[2];
	float p=c[1]*InvC2;
	float q=c[0]*InvC2;
	float D=(p*p*0.25f)-q;
	float r=sqrt(D);
	z[1]=-p*0.5f-r;
	z[2]=-p*0.5f+r;
	// Compute the absorbance by summing the appropriate weights
	float3 polynomial;
	float3 weight_factor = float3(overestimation, (z[1] < z[0])?1.0f:0.0f, (z[2] < z[0])?1.0f:0.0f);
	float f0=weight_factor[0];
	float f1=weight_factor[1];
	float f2=weight_factor[2];
	float f01=(f1-f0)/(z[1]-z[0]);
	float f12=(f2-f1)/(z[2]-z[1]);
	float f012=(f12-f01)/(z[2]-z[0]);
	polynomial[0]=f012;
	polynomial[1]=polynomial[0];
	polynomial[0]=f01-polynomial[0]*z[1];
	polynomial[2]=polynomial[1];
	polynomial[1]=polynomial[0]-polynomial[1]*z[0];
	polynomial[0]=f0-polynomial[0]*z[0];
	float absorbance = polynomial[0] + dot(b.xy, polynomial.yz);;
	// Turn the normalized absorbance into transmittance
	return saturate(exp(-b0 * absorbance));
}