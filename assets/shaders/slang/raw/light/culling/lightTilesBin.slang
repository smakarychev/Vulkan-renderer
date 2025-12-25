implementing lightCulling;

import "core/attributes";
import "core/lib";
import "core/viewInfo";
import "light/light";
import "utility/fullscreen";

struct CreateTilesResources {
    ConstantBuffer<ViewInfo> view;
    RWStructuredBuffer<LightTile> tiles;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void setupTiles(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CreateTilesResources> resources) {

    const uint2 tileId = dispatchThreadID.xy;
    const uint2 tileCount = resources.view.shading.lightCullTileCount;
    
    if (tileId.x >= tileCount.x || tileId.y >= tileCount.y) 
        return;

    const float2 tileSize = float2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y);
    
    float3 viewSpace[7];
    viewSpace[0] = resources.view.camera.screenToView(float3((tileId + uint2(0, 0)) * tileSize, 1));
    viewSpace[1] = resources.view.camera.screenToView(float3((tileId + uint2(1, 0)) * tileSize, 1));
    viewSpace[2] = resources.view.camera.screenToView(float3((tileId + uint2(0, 1)) * tileSize, 1));
    viewSpace[3] = resources.view.camera.screenToView(float3((tileId + uint2(1, 1)) * tileSize, 1));
    viewSpace[4] = resources.view.camera.screenToView(float3((tileId + uint2(0, 0)) * tileSize, 1));
    viewSpace[5] = resources.view.camera.screenToView(float3((tileId + uint2(1, 0)) * tileSize, 1));
    viewSpace[6] = resources.view.camera.screenToView(float3((tileId + uint2(0, 1)) * tileSize, 1));
    [unroll]
    for (uint i = 0; i < 4; i++) {
        viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], -resources.view.camera.near);
    }
    [unroll]
    for (uint i = 4; i < 7; i++) {
        viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], -resources.view.shading.maxLightCullDistance);
    }


    LightTile tile;
    // left
    tile.planes[0] = Plane(viewSpace[2], viewSpace[0], viewSpace[4]);
    // right
    tile.planes[1] = Plane(viewSpace[1], viewSpace[3], viewSpace[5]);
    // top
    tile.planes[2] = Plane(viewSpace[0], viewSpace[1], viewSpace[4]);
    // bottom
    tile.planes[3] = Plane(viewSpace[3], viewSpace[2], viewSpace[6]);
    for (uint i = 0; i < BIN_COUNT; i++)
        tile.bins[i] = 0;

    resources.tiles[tileId.x + tileId.y * tileCount.x] = tile;
}

struct BinLightTilesResources {
    RWStructuredBuffer<LightTile> tiles;
    StructuredBuffer<PointLight> pointLights;
    ConstantBuffer<ViewInfo> view;
    Texture2D depth;
}

struct AABB {
    float4 min;
    float4 max;
};

groupshared uint2 sDepthRange;
groupshared AABB sAabb;
groupshared uint sDepthMask;

[shader("compute")]
[numthreads(BIN_LIGHTS_TILES_WORKGROUP, BIN_LIGHTS_TILES_WORKGROUP, 1)]
void binLightsTiles(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint2 groupThreadID: SV_GroupThreadID,
    uint2 groupID: SV_GroupID,
    uniform ParameterBlock<BinLightTilesResources> resources) {

    if (groupIndex == 0) {
        sDepthRange = uint2(~0, 0);
        sDepthMask = 0;
    }

    const uint2 tileCount = resources.view.shading.lightCullTileCount;
    const uint2 tileSize = uint2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y);
    
    // determine the min and max depth per tile
    float depthMin = 1.0f;
    float depthMax = 0.0f;

    float depthSamplesLocal[BIN_LIGHTS_TILES_PER_WORKGROUP_SQ];

    [unroll]
    for (uint tile = 0; tile < BIN_LIGHTS_TILES_PER_WORKGROUP_SQ; tile++) {
        uint2 coord = dispatchThreadID * BIN_LIGHTS_TILES_PER_WORKGROUP.xy + unflatten2d(tile, BIN_LIGHTS_TILES_PER_WORKGROUP);
        coord = min(coord, uint2(resources.view.camera.resolution) - 1);
        const float depth = resources.depth[coord].r;
        depthMin = min(depthMin, depth);
        depthMax = max(depthMax, depth);
        depthSamplesLocal[tile] = depth;
    }
    GroupMemoryBarrierWithGroupSync();
    
    depthMin = WaveActiveMin(depthMin);
    depthMax = WaveActiveMax(depthMax);

    if (WaveIsFirstLane()) {
        InterlockedMin(sDepthRange.x, asuint(depthMin));
        InterlockedMax(sDepthRange.y, asuint(depthMax));
    }
    GroupMemoryBarrierWithGroupSync();
    
    depthMin = asfloat(sDepthRange.y);
    depthMax = asfloat(sDepthRange.x);
    
    // form aabb
    const float depthMinView = Camera.linearizeReverseZInf(depthMin, resources.view.camera.near);
    const float depthMaxView = max(
        -resources.view.shading.maxLightCullDistance, 
        Camera.linearizeReverseZInf(depthMax, resources.view.camera.near)
    );

    if (groupIndex == 0) {
        float3 viewSpace[8];
        viewSpace[0] = resources.view.camera.screenToView(float3((groupID.xy + uint2(0, 0)) * tileSize, 1));
        viewSpace[1] = resources.view.camera.screenToView(float3((groupID.xy + uint2(1, 0)) * tileSize, 1));
        viewSpace[2] = resources.view.camera.screenToView(float3((groupID.xy + uint2(0, 1)) * tileSize, 1));
        viewSpace[3] = resources.view.camera.screenToView(float3((groupID.xy + uint2(1, 1)) * tileSize, 1));
        viewSpace[4] = resources.view.camera.screenToView(float3((groupID.xy + uint2(0, 0)) * tileSize, 1));
        viewSpace[5] = resources.view.camera.screenToView(float3((groupID.xy + uint2(1, 0)) * tileSize, 1));
        viewSpace[6] = resources.view.camera.screenToView(float3((groupID.xy + uint2(0, 1)) * tileSize, 1));
        viewSpace[7] = resources.view.camera.screenToView(float3((groupID.xy + uint2(1, 1)) * tileSize, 1));
        [unroll]
        for (uint i = 0; i < 4; i++) {
            viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], depthMinView);
        }
        [unroll]
        for (uint i = 4; i < 8; i++) {
            viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], depthMaxView);
        }

        float3 aabbMin = 1e+11;
        float3 aabbMax = -1e+11;

        [unroll]
        for (uint i = 0; i < 8; i++) {
            aabbMin = min(aabbMin, viewSpace[i]);
            aabbMax = max(aabbMax, viewSpace[i]);
        }

        sAabb.min = float4(aabbMin, 0.0f);
        sAabb.max = float4(aabbMax, 0.0f);
    }
    
    // 2.5d culling
    const float depthRangeInverse = 31.0f / (depthMaxView - depthMinView);
    for (uint i = 0; i < BIN_LIGHTS_TILES_PER_WORKGROUP_SQ; i++) {
        const float depthView = Camera.linearizeReverseZ(depthSamplesLocal[i], resources.view.camera.near,
            resources.view.shading.maxLightCullDistance);
        const uint depthBit = uint(max(0.0f, min(31.0f, floor((depthView - depthMinView) * depthRangeInverse))));
        const uint subgoupMask = WaveActiveBitOr(1 << depthBit);
        if (WaveIsFirstLane())
            InterlockedOr(sDepthMask, subgoupMask);
    }
    GroupMemoryBarrierWithGroupSync();

    const uint tileIndex = groupID.x + groupID.y * tileCount.x;
    const LightTile tile = resources.tiles[tileIndex];

    // finally, cull the lights
    for (uint i = groupIndex; i < resources.view.shading.pointLightCount; i += BIN_LIGHTS_TILES_WORKGROUP * BIN_LIGHTS_TILES_WORKGROUP) {
        const PointLight light = resources.pointLights[i];
        const float radius = light.radius;
        float3 position = light.position;
        position = mul(float4(position, 1.0f), resources.view.camera.view).xyz;
        
        if (isInTileFrustum(tile, position, radius, resources.view.camera.near, resources.view.shading.maxLightCullDistance)) {
            const uint binIndex = i / BIN_BIT_SIZE;
            const uint binIndexMin = i % BIN_BIT_SIZE;
            if (lightCulling.distanceAabbSquared(position, sAabb.min, sAabb.max) <= radius * radius) {
                if ((lightMask(position, radius, depthMinView, depthRangeInverse) & sDepthMask) != 0) {
                    InterlockedOr(resources.tiles[tileIndex].bins[binIndex], 1 << binIndexMin);
                }
            }
        }
    }
}

bool isInsidePlane(const Plane plane, float3 position, float radius) {
    return dot(plane.plane.xyz, position) - plane.plane.w < radius;
}

bool isInTileFrustum(const LightTile tile, float3 position, float radius, float near, float far) {
    bool isInTile = true;
    isInTile = isInTile && position.z - radius < -near;
    isInTile = isInTile && position.z + radius > -far;
    isInTile = isInTile && isInsidePlane(tile.planes[0], position, radius);
    isInTile = isInTile && isInsidePlane(tile.planes[1], position, radius);
    isInTile = isInTile && isInsidePlane(tile.planes[2], position, radius);
    isInTile = isInTile && isInsidePlane(tile.planes[3], position, radius);
    
    return isInTile;
}

uint lightMask(float3 position, float radius, float depthAabbMin, float depthRangeInverse) {
    const float depthMin = position.z + radius;
    const float depthMax = position.z - radius;
    
    const uint depthMaskMin = uint(max(0.0f, min(31.0f, floor((depthMin - depthAabbMin) * depthRangeInverse))));
    const uint depthMaskMax = uint(max(0.0f, min(31.0f, floor((depthMax - depthAabbMin) * depthRangeInverse))));
    
    uint mask = ~0;
    mask = mask >> 31 - (depthMaskMax - depthMaskMin);
    mask = mask << depthMaskMin;
    
    return mask;
}

struct VisualizeLightTilesSamplers {
    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState sampler;
}

struct VisualizeLightTilesResources {
    StructuredBuffer<LightTile> tiles;
    StructuredBuffer<LightZBin> zbins;
    ConstantBuffer<ViewInfo> view;
    Texture2D depth;
}

[shader("pixel")]
float4 visualizeLightTiles(
    FullScreenVSOutput input,
    uniform ParameterBlock<VisualizeLightTilesSamplers> samplers,
    uniform ParameterBlock<VisualizeLightTilesResources> resources) {

    const float depth = resources.depth.SampleLevel(samplers.sampler, input.uv, 0).r;
    
    uint binMin = 0;
    uint binMax = BIN_COUNT - 1;

    if (resources.view.shading.lightCullingUseZBins) {
        const uint zbinIndex = lightCulling.getZBinIndex(depth, resources.view.camera.near,
            resources.view.shading.maxLightCullDistance);
        const uint lightMin = uint(resources.zbins[zbinIndex].min);
        const uint lightMax = uint(resources.zbins[zbinIndex].max);
        binMin = lightMin / BIN_BIT_SIZE;
        binMax = binMax / BIN_BIT_SIZE;
    }
    
    const uint tileIndex = lightCulling.getTileIndex(input.uv, resources.view.camera.resolution, 
        resources.view.shading.lightCullTileCount);
    
    const LightTile tile = resources.tiles[tileIndex];
    uint lightCount = 0;
    for (uint i = binMin; i <= binMax; i++) {
        lightCount += countbits(tile.bins[i]);
    }
    
    const uint MAX_LIGHTS_TO_COLOR = 32;
    return float4(colorHeatMap(saturate(float(lightCount) / MAX_LIGHTS_TO_COLOR)), 1.0f);
}

float3 visualizeTilesColor(float t) {
    const float3 a = float3(0.5f, 0.5f, 0.5f);		
    const float3 b = float3(0.5f, 0.5f, 0.5f);	
    const float3 c = float3(1.0f, 1.0f, 1.0f);	
    const float3 d = float3(0.3f, 0.2f, 0.2f);
    
    return a + b * cos(2.0f * 3.1415f * (c * t + d));
}

float3 colorHeatMap(float t) {
    t = t * 3.1415 * 0.5f;
    return float3(sin(t), sin(2.0f * t), cos(t));
}