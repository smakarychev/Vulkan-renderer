implementing lightCulling;

import "core/attributes";
import "core/geometry";
import "core/lib";
import "core/viewInfo";
import "light/light";
import "utility/fullscreen";

struct CreateClustersResources {
    ConstantBuffer<ViewInfo> view;
    RWStructuredBuffer<LightCluster> clusters;
    RWStructuredBuffer<uint8_t> visibility;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void setupClusters(
    uint3 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CreateClustersResources> resources) {

    const uint3 clusterSize = uint3(float3(
        resources.view.camera.resolution,
        resources.view.shading.maxLightCullDistance - resources.view.camera.near) / 
        uint3(LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z));

    const uint3 clusterId = dispatchThreadID;
    if (
        clusterId.x >= LIGHT_CLUSTER_BINS_X || 
        clusterId.y >= LIGHT_CLUSTER_BINS_Y ||
        clusterId.z >= LIGHT_CLUSTER_BINS_Z)
        return;
    
    float3 viewSpace[8];
    viewSpace[0] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(0, 0)) * clusterSize.xy, 1));
    viewSpace[1] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(1, 0)) * clusterSize.xy, 1));
    viewSpace[2] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(0, 1)) * clusterSize.xy, 1));
    viewSpace[3] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(1, 1)) * clusterSize.xy, 1));
    viewSpace[4] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(0, 0)) * clusterSize.xy, 1));
    viewSpace[5] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(1, 0)) * clusterSize.xy, 1));
    viewSpace[6] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(0, 1)) * clusterSize.xy, 1));
    viewSpace[7] = resources.view.camera.screenToView(float3((clusterId.xy + uint2(0, 1)) * clusterSize.xy, 1));

    const float clusterNear = 
        getCascadeDepth(clusterId.z, resources.view.camera.near, resources.view.shading.maxLightCullDistance);
    const float clusterFar = 
        getCascadeDepth(clusterId.z + 1, resources.view.camera.near, resources.view.shading.maxLightCullDistance);

    [unroll]
    for (uint i = 0; i < 4; i++) {
        viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], -clusterNear);
    }
    [unroll]
    for (uint i = 4; i < 8; i++) {
        viewSpace[i] = lightCulling.linePlaneIntersection(viewSpace[i], -clusterFar);
    }

    float3 aabbMin = 1e+11;
    float3 aabbMax = -1e+11;

    [unroll]
    for (uint i = 0; i < 8; i++) {
        aabbMin = min(aabbMin, viewSpace[i]);
        aabbMax = max(aabbMax, viewSpace[i]);
    }

    uint clusterIndex = flatten3d(clusterId, uint3(LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z));
    resources.clusters[clusterIndex].min = float4(aabbMin, 0.0f);
    resources.clusters[clusterIndex].max = float4(aabbMax, 0.0f);
    for (uint i = 0; i < BIN_COUNT; i++) 
        resources.clusters[clusterIndex].bins[i] = 0;
    resources.visibility[clusterIndex] = 0;
}

float getCascadeDepth(uint zIndex, float n, float f) {
    return n * pow(f / n, float(zIndex) / float(LIGHT_CLUSTER_BINS_Z));
}

struct CompactIdentifyClustersResources {
    ConstantBuffer<ViewInfo> view;
    RWStructuredBuffer<uint8_t> visibility;
    Texture2D<float> depth;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compactIdentifyClusters(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CompactIdentifyClustersResources> resources) {

    const uint2 coord = dispatchThreadID;
    const float depth = resources.depth[coord];
    if (depth == 0)
        return;

    const float2 uv = (float2(coord) + 0.5f) / resources.view.camera.resolution;

    const uint slice = lightCulling.sliceIndex(depth,
        resources.view.camera.near, resources.view.shading.maxLightCullDistance);
    const uint clusterIndex = lightCulling.getClusterIndex(uv, slice);
    resources.visibility[clusterIndex] = 1;
}

struct CompactClustersResources {
    StructuredBuffer<uint8_t> visibility;
    RWStructuredBuffer<uint16_t> activeClusters;
    RWStructuredBuffer<uint> activeClustersCount;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compactClusters(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CompactClustersResources> resources) {

    const uint x = dispatchThreadID.x;
    const uint y = dispatchThreadID.y % LIGHT_CLUSTER_BINS_Y;
    const uint z = dispatchThreadID.y / LIGHT_CLUSTER_BINS_Y;

    if (x >= LIGHT_CLUSTER_BINS_X || y >= LIGHT_CLUSTER_BINS_Y || z >= LIGHT_CLUSTER_BINS_Z)
        return;

    const uint clusterIndex = 
        flatten3d(uint3(x, y, z), uint3(LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z));

    const bool isActive = resources.visibility[clusterIndex] == 1;
    const uint totalActiveCount = WaveActiveCountBits(isActive);
    uint compactedBufferIndexBase;
    if (WaveIsFirstLane())
        InterlockedAdd(resources.activeClustersCount[0], totalActiveCount, compactedBufferIndexBase);
    compactedBufferIndexBase = WaveReadLaneFirst(compactedBufferIndexBase);

    const uint compactedBufferOffset = WavePrefixCountBits(isActive);
    const uint compactedBufferIndex = compactedBufferIndexBase + compactedBufferOffset;

    if (isActive) 
        resources.activeClusters[compactedBufferIndex] = (uint16_t)clusterIndex;
}

struct CompactCreateDispatchClustersResources {
    ConstantBuffer<uint> activeClustersCount;
    RWStructuredBuffer<IndirectDispatchCommand> command;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void compactCreateDispatchClusters(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CompactCreateDispatchClustersResources> resources) {
    
    resources.command[0].x = (resources.activeClustersCount + BIN_DISPATCH_SIZE - 1) / BIN_DISPATCH_SIZE;
    resources.command[0].y = resources.command[0].z = 1;
}

struct BinClustersResources {
    RWStructuredBuffer<LightCluster> clusters;
    StructuredBuffer<uint16_t> activeClusters;
    ConstantBuffer<uint> activeClustersCount;
    StructuredBuffer<PointLight> pointLights;
    ConstantBuffer<ViewInfo> view;
}

groupshared float3 sPointLightsPosition[BIN_DISPATCH_SIZE];
groupshared float sPointLightsRadius[BIN_DISPATCH_SIZE];

[shader("compute")]
[numthreads(BIN_DISPATCH_SIZE, 1, 1)]
void binLightsClusters(
    uint dispatchThreadID: SV_DispatchThreadID,
    uint groupThreadID: SV_GroupThreadID,
    uniform ParameterBlock<BinClustersResources> resources) {
    
    const uint clusterIndex = uint(resources.activeClusters[dispatchThreadID]);
    const LightCluster cluster = resources.clusters[clusterIndex];

    // load lights into shared memory in batches
    const uint batchCount = (
        min(VIEW_MAX_LIGHTS,
        resources.view.shading.pointLightCount) + BIN_DISPATCH_SIZE - 1) / BIN_DISPATCH_SIZE;
    for (uint batch = 0; batch < batchCount; batch++) {
        const uint lightOffset = batch * BIN_DISPATCH_SIZE;
        const uint lightFetchIndex = groupThreadID + lightOffset;
        if (lightFetchIndex < resources.view.shading.pointLightCount) {
            sPointLightsPosition[groupThreadID] = resources.pointLights[lightFetchIndex].position;
            sPointLightsRadius[groupThreadID] = resources.pointLights[lightFetchIndex].radius;
        }
        GroupMemoryBarrierWithGroupSync();

        if (dispatchThreadID < resources.activeClustersCount) {
            // go through each light, determine the cluster it belongs to
            uint lightCount = min(BIN_DISPATCH_SIZE, resources.view.shading.pointLightCount - lightOffset);
            for (uint lightIndex = lightOffset; lightIndex < lightOffset + lightCount; lightIndex++) {
                const float radius = sPointLightsRadius[lightIndex - lightOffset];
                float3 position = sPointLightsPosition[lightIndex - lightOffset];
                position = mul(float4(position, 1.0f), resources.view.camera.view).xyz;
                const bool inCluster = isInCluster(cluster, position, radius);
                if (inCluster) {
                    const uint binIndex = lightIndex / BIN_BIT_SIZE;
                    const uint binIndexMin = lightIndex % BIN_BIT_SIZE;
                    resources.clusters[clusterIndex].bins[binIndex] |= 1 << binIndexMin;
                }
            }
        }        
        GroupMemoryBarrierWithGroupSync();
    }    
}

bool isInCluster(const LightCluster cluster, float3 position, float radius) {
    const float distance2 = lightCulling.distanceAabbSquared(position, cluster.min, cluster.max);

    return distance2 <= radius * radius;
}

struct VisualizeLightClustersSamplers {
    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState sampler;
}

struct VisualizeLightClustersResources {
    StructuredBuffer<LightCluster> clusters;
    ConstantBuffer<ViewInfo> view;
    Texture2D<float> depth;
}

[shader("pixel")]
float4 visualizeLightClusters(
    FullScreenVSOutput input,
    uniform ParameterBlock<VisualizeLightClustersSamplers> samplers,
    uniform ParameterBlock<VisualizeLightClustersResources> resources) {

    const float depth = resources.depth.SampleLevel(samplers.sampler, input.uv, 0);
    const uint slice = lightCulling.sliceIndex(depth,
        resources.view.camera.near, resources.view.shading.maxLightCullDistance);
    const uint clusterIndex = lightCulling.getClusterIndex(input.uv, slice);
    const LightCluster cluster = resources.clusters[clusterIndex];

    uint lightCount = 0;
    for (uint i = 0; i < BIN_COUNT; i++) {
        lightCount += countbits(cluster.bins[i]);
    }

    const uint MAX_LIGHTS_TO_COLOR = 64;
    return float4(color.heatmap(saturate(float(lightCount) / MAX_LIGHTS_TO_COLOR)), 1.0f);
}


struct VisualizeLightClustersCascadesSamplers {
    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState sampler;
}

struct VisualizeLightClustersCascadesResources {
    ConstantBuffer<ViewInfo> view;
    Texture2D<float> depth;
}

[shader("pixel")]
float4 visualizeLightClustersCascades(
    FullScreenVSOutput input,
    uniform ParameterBlock<VisualizeLightClustersCascadesSamplers> samplers,
    uniform ParameterBlock<VisualizeLightClustersCascadesResources> resources) {

    const float depth = resources.depth.SampleLevel(samplers.sampler, input.uv, 0);
    const uint slice = lightCulling.sliceIndex(depth,
        resources.view.camera.near, resources.view.shading.maxLightCullDistance);

    return float4(color.hash(slice), 1.0f);
}