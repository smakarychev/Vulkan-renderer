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

struct TransmittancePassSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct TransmittancePassResources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneDrawElement> visibleMeshlets;
    StructuredBuffer<StandardPbrMaterial> materials;
    ConstantBuffer<DepthRange> depthRange;
    coherent RasterizerOrderedTexture2D<float16_t> absorbanceTotal;
    coherent RasterizerOrderedTexture2D<float16_t4> absorbanceMoments;
}

struct VSOutput {
    float4 position : SV_POSITION;
    nointerpolation uint materialId;
    float2 uv;
    float viewZ;
};

ParameterBlock<TransmittancePassSamplers> samplers;
ParameterBlock<TransmittancePassResources> resources;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const SceneDrawElement element = resources.visibleMeshlets[baseInstance];
    const RenderObject renderObject = resources.renderObjects[element.renderObjectId];
    const Meshlet meshlet = resources.meshlets.meshlet(renderObject.meshletIndex + element.meshletId);
    const uint vertexOffset = meshlet.firstVertex + vertexID;
    float4x4 transform = renderObject.transform;

    float4 positionWs;

    [branch]
    if (WaveActiveBitOr(renderObject.isDeformed())) {
        SkinnedVertex vertex = resources.ugb.skinnedVertex(renderObject.positionIndex + vertexOffset);
        positionWs = float4(vertex.position, 1.0f);
    }
    else {
        positionWs = mul(float4(resources.ugb.position(renderObject.positionIndex + vertexOffset), 1.0f), transform);
    }

    VSOutput output;
    output.materialId = renderObject.materialId;
    output.position = mul(positionWs, resources.view.camera.viewProjection);
    output.uv = resources.ugb.uv(renderObject.uvIndex + vertexOffset);
    output.viewZ = dot(positionWs, resources.view.camera.view._m02_m12_m22_m32);

    return output;
}

[shader("pixel")]
[earlydepthstencil]
void transmittancePass(VSOutput input, float2 coord : SV_Position) {
    const StandardPbrMaterial material = resources.materials[input.materialId];
    const float4 albedo = material.albedo * 
        nonuniform(material.albedoTexture.desc).Sample(samplers.sampler, input.uv);

    const float transmittance = 1.0f - albedo.a;
    clip(0.9999999f - transmittance);

    const float zMin = asfloat(resources.depthRange.min);
    const float zMax = asfloat(resources.depthRange.max);

    const float z = saturate((log(-input.viewZ) - log(zMin)) / (log(zMax) - log(zMin))) * 2.0f - 1.0f;
    const float z2 = z * z;
    const float z3 = z2 * z;
    const float z4 = z2 * z2;

    const float logAbsorbance = -log(transmittance);

    float b0 = resources.absorbanceTotal[(int2)coord];
    float4 b = resources.absorbanceMoments[(int2)coord];

    float2 bEven = b.yw;
    float2 bOdd = b.xz;
    offsetMoments(bEven, bOdd, -1.0f);
    bEven *= b0;
    bOdd *= b0;

    float2 bEvenQ, bOddQ;
    quantizeMoments(bEvenQ, bOddQ, float2(z2, z4), float2(z, z3));
    b0 += logAbsorbance;
    bEven += bEvenQ * logAbsorbance;
    bOdd += bOddQ * logAbsorbance;
    bEven /= b0;
    bOdd /= b0;
    offsetMoments(bEven, bOdd, 1.0f);

    resources.absorbanceTotal[(int2)coord] = float16_t(b0);
    resources.absorbanceMoments[(int2)coord] = float16_t4(bOdd.x, bEven.x, bOdd.y, bEven.y);
}

void offsetMoments(inout float2 bEven, inout float2 bOdd, float sign) {
    bOdd += 0.5f * sign;
}

void quantizeMoments(out float2 bEvenQ, out float2 bOddQ, float2 bEven, float2 bOdd) {
	bOddQ = mul(bOdd, float2x2(1.5f, sqrt(3.0f) * 0.5f, -2.0f, -sqrt(3.0f) * 2.0f / 9.0f));
	bEvenQ = mul(bEven, float2x2(4.0f, 0.5f, -4.0f, 0.5f));
}