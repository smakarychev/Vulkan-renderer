implementing pbr;

import "core/attributes";
import "light/light";
import "light/culling/lightCulling";

#if !defined(USE_TILED_LIGHTING) &&  !defined(USE_CLUSTERED_LIGHTING) && !defined(USE_HYBRID_LIGHTING)
#define USE_TILED_LIGHTING 1
#endif // !USE_TILED_LIGHTING && !USE_CLUSTERED_LIGHTING && !USE_HYBRID_LIGHTING

[SpecializationConstant] const float MAX_REFLECTION_LOD = 5.0f;

public struct PbrShadingSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState environmentSampler;
    [ImmutableSampler(SamplerFlags.Linear | SamplerFlags.ClampEdge)]
    SamplerState brdfSampler;
}

public struct PbrShadingResources {
    public StructuredBuffer<DirectionalLight> directionalLights;
    public StructuredBuffer<PointLight> pointLights;
    public StructuredBuffer<LightZBin> lightZBins;
    public StructuredBuffer<LightTile> lightTiles;
    public StructuredBuffer<LightCluster> lightClusters;
    public TextureCube prefilteredEnvironment;
    public Texture2D<float2> brdf;
    public ConstantBuffer<SH2Irradiance> irradianceSH;
}

internal float3 pbrShadePointLight(const PbrShadingInfo shadingInfo, const PointLight light) {
    float3 lightDirection = light.position - shadingInfo.position;
    const float falloff = pointLightFalloff(lightDirection, light.radius);
    lightDirection = normalize(lightDirection);

    const float3 illuminance = light.color * light.intensity * falloff;
    const float NoL = clamp(dot(shadingInfo.normal, lightDirection), 1e-5f, 1.0f);

    return BRDF(lightDirection, NoL, shadingInfo) * illuminance * NoL;
}

internal float3 pbrShadePointLightsTiles(const ViewInfo view, const PbrShadingInfo shadingInfo,
    StructuredBuffer<PointLight> lights, StructuredBuffer<LightZBin> zbins, StructuredBuffer<LightTile> tiles) {
    
    float3 Lo = float3(0.0f);
    const uint zbinIndex = lightCulling.getZBinIndex(shadingInfo.depth, view.camera.near, 
        view.shading.maxLightCullDistance);
    const uint tileIndex = lightCulling.getTileIndex(shadingInfo.screenUv, view.camera.resolution,
        view.shading.lightCullTileCount);
    const LightTile tile = tiles[tileIndex];

    const uint lightMin = uint(zbins[zbinIndex].min);
    const uint lightMax = uint(zbins[zbinIndex].max);
    const uint lightMinGroup = WaveActiveMin(lightMin);
    const uint lightMaxGroup = WaveActiveMax(lightMax);

    const uint binMin = lightMinGroup / BIN_BIT_SIZE;
    const uint binMax = lightMaxGroup / BIN_BIT_SIZE;

    for (uint bin = binMin; bin <= binMax; bin++) {
        uint binBits = tile.bins[bin];
        const uint localBitMin = clamp(int(lightMin) - int(bin * 32), 0, int(BIN_BIT_SIZE - 1));
        const uint localBitWidth = clamp(int(lightMax) - int(lightMin) + 1, 0, int(BIN_BIT_SIZE));
        uint maskLocal = ~0u;
        if (localBitWidth != BIN_BIT_SIZE) {
            maskLocal = maskLocal >> (BIN_BIT_SIZE - localBitWidth);
            maskLocal = maskLocal << localBitMin;
        }

        binBits = binBits & maskLocal;
        uint maskGroup = WaveActiveBitOr(binBits);
        while(maskGroup > 0) {
            const uint bitIndex = firstbitlow(maskGroup);
            const uint lightIndex = bin * BIN_BIT_SIZE + bitIndex;
            Lo += pbrShadePointLight(shadingInfo, lights[lightIndex]);
            maskGroup = maskGroup & ~(1u << bitIndex);
        }
    }
    
    return Lo;
}

internal float3 pbrShadePointLightsClusters(const ViewInfo view, const PbrShadingInfo shadingInfo,
    StructuredBuffer<PointLight> lights, StructuredBuffer<LightCluster> clusters) {

    float3 Lo = float3(0.0f);
    const uint slice = 
        lightCulling.sliceIndexLinearDepth(shadingInfo.viewZ, view.camera.near, view.shading.maxLightCullDistance);
    const uint clusterIndex = lightCulling.getClusterIndex(shadingInfo.screenUv, slice);
    const LightCluster cluster = clusters[clusterIndex];
    
    for (uint bin = 0; bin < BIN_COUNT; bin++) {
        const uint maskLocal = cluster.bins[bin];
        uint maskGroup = WaveActiveBitOr(maskLocal);
        while (maskGroup > 0) {
            const uint bitIndex = firstbitlow(maskGroup);
            const uint lightIndex = bin * BIN_BIT_SIZE + bitIndex;
            Lo += pbrShadePointLight(shadingInfo, lights[lightIndex]);
            maskGroup = maskGroup & ~(1u << bitIndex);
        }
    }
    
    return Lo;
}

internal float3 pbrShadePointLightsHybrid(const ViewInfo view, const PbrShadingInfo shadingInfo,
    StructuredBuffer<PointLight> lights, StructuredBuffer<LightZBin> zbins, StructuredBuffer<LightTile> tiles,
    StructuredBuffer<LightCluster> clusters) {

    float3 Lo = float3(0.0f);
    const uint zbinIndex = lightCulling.getZBinIndex(shadingInfo.depth, view.camera.near, 
        view.shading.maxLightCullDistance);
    const uint tileIndex = lightCulling.getTileIndex(shadingInfo.screenUv, view.camera.resolution,
        view.shading.lightCullTileCount);
    const LightTile tile = tiles[tileIndex];
    const uint slice = 
        lightCulling.sliceIndexLinearDepth(shadingInfo.viewZ, view.camera.near, view.shading.maxLightCullDistance);
    const uint clusterIndex = lightCulling.getClusterIndex(shadingInfo.screenUv, slice);
    const LightCluster cluster = clusters[clusterIndex];

    const uint lightMin = uint(zbins[zbinIndex].min);
    const uint lightMax = uint(zbins[zbinIndex].max);
    const uint lightMinGroup = WaveActiveMin(lightMin);
    const uint lightMaxGroup = WaveActiveMax(lightMax);

    const uint binMin = lightMinGroup / BIN_BIT_SIZE;
    const uint binMax = lightMaxGroup / BIN_BIT_SIZE;

    for (uint bin = binMin; bin <= binMax; bin++) {
        uint lightCount;
        uint maskGroup;
        {
            uint binBits = tile.bins[bin];
            const uint localBitMin = clamp(int(lightMin) - int(bin * 32), 0, int(BIN_BIT_SIZE - 1));
            const uint localBitWidth = clamp(int(lightMax) - int(lightMin) + 1, 0, int(BIN_BIT_SIZE));
            uint maskLocal = ~0u;
            if (localBitWidth != BIN_BIT_SIZE) {
                maskLocal = maskLocal >> (BIN_BIT_SIZE - localBitWidth);
                maskLocal = maskLocal << localBitMin;
            }

            binBits = binBits & maskLocal;
            maskGroup = WaveActiveBitOr(binBits);
            lightCount = countbits(maskGroup);
        }
        {
            const uint maskLocal = cluster.bins[bin];
            uint maskGroupClusters = WaveActiveBitOr(maskLocal);
            if (lightCount > countbits(maskGroupClusters))
                maskGroup = maskGroupClusters;
        }

        while(maskGroup > 0) {
            const uint bitIndex = firstbitlow(maskGroup);
            const uint lightIndex = bin * BIN_BIT_SIZE + bitIndex;
            Lo += pbrShadePointLight(shadingInfo, lights[lightIndex]);
            maskGroup = maskGroup & ~(1u << bitIndex);
        }
    }
    
    return Lo;
}

public float3 pbrShadePointLights(const ViewInfo view, const PbrShadingInfo shadingInfo,
    const PbrShadingResources resources) {
    
#if USE_TILED_LIGHTING
    return pbrShadePointLightsTiles(view, shadingInfo,
        resources.pointLights, resources.lightZBins, resources.lightTiles);
#elif USE_CLUSTERED_LIGHTING
    return pbrShadePointLightsClusters(view, shadingInfo, resources.pointLights, resources.lightClusters);
#elif USE_HYBRID_LIGHTING
    return pbrShadePointLightsHybrid(view, shadingInfo, resources.pointLights, resources.lightZBins,
        resources.lightTiles, resources.lightClusters);
#endif // USE_TILED_LIGHTING
}

public float3 pbrShadeDirectionalLight(const ViewInfo view, const PbrShadingInfo shadingInfo,
    const DirectionalLight light, float shadow) {

    const float3 lightDirection = -light.direction;
    const float3 illuminance = light.color * light.intensity;
    const float NoL = clamp(dot(shadingInfo.normal, lightDirection), 1e-5f, 1.0f);

    return BRDF(lightDirection, NoL, shadingInfo) * illuminance * NoL * shadow;
}

public float3 pbrShadeDirectionalLights(const ViewInfo view, const PbrShadingInfo shadingInfo,
    const PbrShadingResources resources) {

    float3 Lo = pbrShadeSun(view, shadingInfo, shadingInfo.directionalShadow) *
        view.shading.atmosphereTransmittanceAtView.rgb;
    for (uint i = 0; i < view.shading.directionalLightCount; i++) {
        Lo += pbrShadeDirectionalLight(view, shadingInfo, resources.directionalLights[i], 1.0f);
    }

    return Lo;
}

public float3 pbrShadeSun(const ViewInfo view, const PbrShadingInfo shadingInfo, float shadow) {
    const float SUN_ANGULAR_RADIUS = 0.263f * PI / 180.0f;
    const float3 lightDirection = -view.shading.primaryDirectionalLightDirection;
    const float sunRadius = sin(SUN_ANGULAR_RADIUS);
    const float distanceToSun = cos(SUN_ANGULAR_RADIUS);
    const float3 R = reflect(-shadingInfo.view, shadingInfo.normal);
    // Gram–Schmidt
    const float DoR = dot(R, lightDirection);
    const float3 S = R - DoR * lightDirection;
    // if reflection direction already points at the sun disk, use it as a light direction
    // otherwise, use the closest point on the disk
    const float3 L = DoR < distanceToSun ? normalize(distanceToSun * lightDirection + normalize(S) * sunRadius) : R;

    const float3 illuminance = 
        view.shading.primaryDirectionalLightColor.rgb * 
        view.shading.primaryDirectionalLightIntensity;
    const float NoL = clamp(dot(shadingInfo.normal, lightDirection), 1e-5f, 1.0f);

    return BRDF(L, NoL, shadingInfo) * illuminance * NoL * shadow;
}

public float3 pbrShadeIbl(const ViewInfo view, const PbrShadingInfo shadingInfo, const PbrShadingSamplers samplers,
    const PbrShadingResources resources) {

    const float3 R = reflect(-shadingInfo.view, shadingInfo.normal);
    const float3 irradiance = resources.irradianceSH.shade(shadingInfo.normal);
    
    const float linearRoughness = sqrt(shadingInfo.roughness);
    const float lod = linearRoughness * MAX_REFLECTION_LOD;
    const float3 prefiltered = resources.prefilteredEnvironment.SampleLevel(samplers.environmentSampler, R, lod).rgb;
    const float2 brdf = 
        resources.brdf.SampleLevel(samplers.brdfSampler, float2(shadingInfo.NoV, linearRoughness), 0.0f).rg;

    const float3 diffuse = irradiance * shadingInfo.diffuseColor;
    const float3 specular = (shadingInfo.F0 * brdf.x + shadingInfo.F90 * brdf.y) * prefiltered;

    const float3 ambient = specular + diffuse;

    return ambient * shadingInfo.ao;
}


public float3 pbrShade(const ViewInfo view, const PbrShadingInfo shadingInfo, const PbrShadingSamplers samplers,
    const PbrShadingResources resources) {

    float3 Lo = pbrShadePointLights(view, shadingInfo, resources);
    Lo += pbrShadeDirectionalLights(view, shadingInfo, resources);
    Lo *= view.shading.exposure;
    Lo += pbrShadeIbl(view, shadingInfo, samplers, resources);

    return Lo;
}