// this is definitely not the fastest shader...

#version 460

#include "common.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;

layout(set = 1, binding = 1) readonly buffer clusters {
    Cluster clusters[];
} u_clusters;

layout(set = 1, binding = 2) uniform camera {
    CameraGPU camera;
} u_camera;

vec3 color(float t) {
    const vec3 a = vec3(0.5f, 0.5f, 0.5f);		
    const vec3 b = vec3(0.5f, 0.5f, 0.5f);	
    const vec3 c = vec3(1.0f, 1.0f, 1.0f);	
    const vec3 d = vec3(0.3f, 0.2f, 0.2f);
    
    return a + b * cos(2.0f * 3.1415f * (c * t + d));
}

void main() {
    const float depth = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    const uint slice = slice_index(depth, u_camera.camera.near, u_camera.camera.far, LIGHT_CLUSTER_BINS_Z);
    const uint cluster_index = get_cluster_index(vertex_uv, slice);

    const Cluster cluster = u_clusters.clusters[cluster_index];
    uint light_count = 0;
    for (uint i = 0; i < BIN_COUNT; i++) {
        light_count += bitCount(cluster.bins[i]);
    }
    
    const uint MAX_LIGHTS_TO_COLOR = 64;
    out_color = vec4(color(clamp(float(light_count) / MAX_LIGHTS_TO_COLOR, 0.0f, 1.0f)), 1.0f);
}