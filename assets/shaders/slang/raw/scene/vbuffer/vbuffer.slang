module vbuffer;

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/ugb";
import "core/viewInfo";

public struct VBufferSamplers {
    [ImmutableSampler(SamplerFlags.Nearest)]
    public SamplerState sampler;
}

public struct VBufferResources {
    public Texture2D<int> vbuffer;
}

/// Represents the instance-triangle information, stored in vbuffer
public struct VBufferVisibility {
    static const uint TRIANGLE_MASK = (1 << 8) - 1;
    static const uint TRIANGLE_BITS = 8;

    public uint instance;
    public uint triangle;
    
    public __init(uint instance, uint triangle) {
        this.instance = instance;
        this.triangle = triangle;
    }

    public __init(uint visibility) {
        instance = visibility >> TRIANGLE_BITS;
        triangle = visibility & TRIANGLE_MASK;
    }

    public bool isValid() {
        return instance != (~0u >> TRIANGLE_BITS);
    }

    public uint pack() {
        return (instance << TRIANGLE_BITS) | triangle;
    }
}

/// Represents interpolated geometry attributes data
public struct VBufferGeometryData {
    public float3 position;
    public float2 uv;
    public float2 uvDx;
    public float2 uvDy;
    public float3 tangent;
    public float3 bitangent;
    public float3 normal;
    public float viewZ;
    public float depth;
    public uint materialId;

    struct InterpolationData {
        float3 barycentric;
        float3 ddx;
        float3 ddy;
    }

    public __init(const VBufferVisibility visibility, const UGB ugb, const ViewInfo view, float2 screenPosition,
        StructuredBuffer<IndirectCommand> commands, StructuredBuffer<RenderObject> objects,
        StructuredBuffer<uint8_t> indices) {

        const IndirectCommand command = commands[visibility.instance];
        const RenderObject renderObject = objects[command.renderObject];

        const uint3 indices = uint3(
            command.vertexOffset + uint(indices[command.firstIndex + visibility.triangle * 3 + 0]),
            command.vertexOffset + uint(indices[command.firstIndex + visibility.triangle * 3 + 1]),
            command.vertexOffset + uint(indices[command.firstIndex + visibility.triangle * 3 + 2])
        );

        float3x4 positions = float3x4(
            float4(ugb.position(renderObject.positionIndex + indices.x), 1.0f),
            float4(ugb.position(renderObject.positionIndex + indices.y), 1.0f),
            float4(ugb.position(renderObject.positionIndex + indices.z), 1.0f),
        );
        transformPositionsToClip(positions, view.camera.viewProjection, renderObject.transform);

        const InterpolationData interpolationData = 
            calculateInterpolation(positions, screenPosition, view.camera.resolution);

        const float w = dot(float3(positions[0].w, positions[1].w, positions[2].w), interpolationData.barycentric);
        const float z = -w * view.camera.projection[2][2] + view.camera.projection[3][2];
        float4 positionI = mul(float4(screenPosition * w, z, w), view.camera.inverseProjection);
        viewZ = positionI.z;
        depth = z / w;
        positionI = mul(positionI, view.camera.inverseView);
        position = positionI.xyz;

        float3x2 uvs = float3x2(
            ugb.uv(renderObject.uvIndex + indices.x),
            ugb.uv(renderObject.uvIndex + indices.y),
            ugb.uv(renderObject.uvIndex + indices.z),
        );
        uvs = interpolateWithDerivatives2d(interpolationData, uvs);
        uv = uvs[0];
        uvDx = uvs[1];
        uvDy = uvs[2];

        float3x3 normals = float3x3(
            ugb.normal(renderObject.normalIndex + indices.x),
            ugb.normal(renderObject.normalIndex + indices.y),
            ugb.normal(renderObject.normalIndex + indices.z),
        );
        normal = interpolate3d(interpolationData, normals);

        float3x3 tangents = float3x3(
            ugb.tangent(renderObject.tangentIndex + indices.x).xyz,
            ugb.tangent(renderObject.tangentIndex + indices.y).xyz,
            ugb.tangent(renderObject.tangentIndex + indices.z).xyz,
        );
        const float handedness = ugb.tangent(renderObject.tangentIndex + indices.x).w;
        tangents = interpolateWithDerivatives3d(interpolationData, tangents);
        tangent = interpolate3d(interpolationData, tangents);

        normal = normalize(mul(normal, float3x3(renderObject.transform)));
        tangent = normalize(mul(tangent, float3x3(renderObject.transform)));
        tangent = normalize(tangent - dot(tangent, normal) * normal);
        bitangent = normalize(cross(normal, tangent)) * handedness;

        materialId = renderObject.materialId;
    }

    static void transformPositionsToClip(inout float3x4 positions, float4x4 viewProjection, float4x4 model) {
        positions[0] = mul(mul(positions[0], model), viewProjection);
        positions[1] = mul(mul(positions[1], model), viewProjection);
        positions[2] = mul(mul(positions[2], model), viewProjection);
    }

    static InterpolationData calculateInterpolation(float3x4 positions, float2 screenPosition, float2 screenResolution) {
        const float3 oneOverW = 1.0f / float3(positions[0].w, positions[1].w, positions[2].w);

        const float2 ndcA = positions[0].xy * oneOverW.x;
        const float2 ndcB = positions[1].xy * oneOverW.y;
        const float2 ndcC = positions[2].xy * oneOverW.z;

        const float inverseDet = 1.0f / determinant(float2x2(ndcC - ndcB, ndcA - ndcB));
        float3 ddx = float3(ndcB.y - ndcC.y, ndcC.y - ndcA.y, ndcA.y - ndcB.y) * inverseDet * oneOverW;
        float3 ddy = float3(ndcC.x - ndcB.x, ndcA.x - ndcC.x, ndcB.x - ndcA.x) * inverseDet * oneOverW;
        float ddxSum = dot(ddx, float3(1.0f));
        float ddySum = dot(ddy, float3(1.0f));

        const float2 deltaVec = screenPosition - ndcA;
        const float interpolatedInvW = oneOverW.x + deltaVec.x * ddxSum + deltaVec.y * ddySum;
        const float interpolatedW = 1.0f / interpolatedInvW;

        float3 lambda;
        lambda.x = interpolatedW * (oneOverW.x + deltaVec.x * ddx.x + deltaVec.y * ddy.x);
        lambda.y = interpolatedW * (0.0f       + deltaVec.x * ddx.y + deltaVec.y * ddy.y);
        lambda.z = interpolatedW * (0.0f       + deltaVec.x * ddx.z + deltaVec.y * ddy.z);

        ddx *= (2.0f / screenResolution.x);
        ddy *= (2.0f / screenResolution.y);
        ddxSum *= (2.0f / screenResolution.x);
        ddySum *= (2.0f / screenResolution.y);

        const float interpolatedWDdx = 1.0f / (interpolatedInvW + ddxSum);
        const float interpolatedWDdy = 1.0f / (interpolatedInvW + ddySum);

        ddx = interpolatedWDdx * (lambda * interpolatedInvW + ddx) - lambda;
        ddy = interpolatedWDdy * (lambda * interpolatedInvW + ddy) - lambda;

        InterpolationData interpolationData;
        interpolationData.barycentric = lambda;
        interpolationData.ddx = ddx;
        interpolationData.ddy = ddy;

        return interpolationData;
    }

    static float interpolate(const InterpolationData interpolation, float3 attributes) {
        float result = dot(attributes, interpolation.barycentric);

        return result;
    }

    static float3 interpolateWithDerivatives(const InterpolationData interpolation, float3 attributes) {
        float3 result;
        result[0] = dot(attributes, interpolation.barycentric);
        result[1] = dot(attributes, interpolation.ddx);
        result[2] = dot(attributes, interpolation.ddy);

        return result;
    }

    static float3x2 interpolateWithDerivatives2d(const InterpolationData interpolation, float3x2 attributes) {
        const float3 attributeX = float3(attributes[0].x, attributes[1].x, attributes[2].x);
        const float3 attributeY = float3(attributes[0].y, attributes[1].y, attributes[2].y);

        const float3 attributeXInterpolated = interpolateWithDerivatives(interpolation, attributeX);
        const float3 attributeYInterpolated = interpolateWithDerivatives(interpolation, attributeY);

        const float ax = attributeXInterpolated[0];
        const float ay = attributeYInterpolated[0];

        const float addxx = attributeXInterpolated[1];
        const float addxy = attributeYInterpolated[1];

        const float addyx = attributeXInterpolated[2];
        const float addyy = attributeYInterpolated[2];

        return float3x2(float2(ax, ay), float2(addxx, addxy), float2(addyx, addyy));
    }

    

    static float3 interpolate3d(const InterpolationData interpolation, float3x3 attributes) {
        const float3 attributeX = float3(attributes[0].x, attributes[1].x, attributes[2].x);
        const float3 attributeY = float3(attributes[0].y, attributes[1].y, attributes[2].y);
        const float3 attributeZ = float3(attributes[0].z, attributes[1].z, attributes[2].z);

        const float ax = interpolate(interpolation, attributeX);
        const float ay = interpolate(interpolation, attributeY);
        const float az = interpolate(interpolation, attributeZ);

        return float3(ax, ay, az);
    }

    static float3x3 interpolateWithDerivatives3d(const InterpolationData interpolation, float3x3 attributes) {
        const float3 attributeX = float3(attributes[0].x, attributes[1].x, attributes[2].x);
        const float3 attributeY = float3(attributes[0].y, attributes[1].y, attributes[2].y);
        const float3 attributeZ = float3(attributes[0].z, attributes[1].z, attributes[2].z);

        const float3 attributeXInterpolated = interpolateWithDerivatives(interpolation, attributeX);
        const float3 attributeYInterpolated = interpolateWithDerivatives(interpolation, attributeY);
        const float3 attributeZInterpolated = interpolateWithDerivatives(interpolation, attributeZ);

        const float ax = attributeXInterpolated[0];
        const float ay = attributeYInterpolated[0];
        const float az = attributeZInterpolated[0];

        const float addxx = attributeXInterpolated[1];
        const float addxy = attributeYInterpolated[1];
        const float addxz = attributeZInterpolated[1];

        const float addyx = attributeXInterpolated[2];
        const float addyy = attributeYInterpolated[2];
        const float addyz = attributeZInterpolated[2];

        return float3x3(float3(ax, ay, az), float3(addxx, addxy, addxz), float3(addyx, addyy, addyz));
    }
}

/// Represents standard pbr material data
public struct VBufferStandardPbrMaterialData {
    public float4 albedo;
    public float3 emissive;
    public float metallic;
    public float roughness;
    public float ao;
    public float3 normal;

    public __init(const VBufferGeometryData geometryData, const StandardPbrMaterial material,
        SamplerState sampler) {
        albedo = material.albedo;
        albedo *= nonuniform(material.albedoTexture.desc).SampleGrad(
            sampler, geometryData.uv, geometryData.uvDx, geometryData.uvDy);

        emissive = nonuniform(material.emissiveTexture.desc).SampleGrad(
            sampler, geometryData.uv, geometryData.uvDx, geometryData.uvDy).rgb;

        const float2 metallicRoughness = nonuniform(material.metallicRoughnessTexture.desc).SampleGrad(
            sampler, geometryData.uv, geometryData.uvDx, geometryData.uvDy).rg;
        metallic = metallicRoughness.r * material.metallic;
        roughness = metallicRoughness.g * material.roughness;
        ao = nonuniform(material.ambientOcclusionTexture.desc).SampleGrad(
            sampler, geometryData.uv, geometryData.uvDx, geometryData.uvDy).r;
        
        normal = nonuniform(material.normalTexture.desc).SampleGrad(
            sampler, geometryData.uv, geometryData.uvDx, geometryData.uvDy).rgb;
        normal = normalize(normal * 2.0f - 1.0f);
        normal = geometryData.tangent * normal.x + geometryData.bitangent * normal.y + geometryData.normal * normal.z;
    }
}