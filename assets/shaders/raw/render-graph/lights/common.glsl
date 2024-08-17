#include "../../light.glsl"

uint slice_index(float depth, float n, float f, uint count) {
    const float z = (n - f) * depth - n;
    const float log_f_n_inv = 1.0f / log(f / n);

    return uint(floor(log(f) * count * log_f_n_inv - log(-z) * count * log_f_n_inv));
}

// todo: make defines, so i can set them on compilation
const uint LIGHT_CLUSTER_BINS_X = 16;
const uint LIGHT_CLUSTER_BINS_Y = 9;
const uint LIGHT_CLUSTER_BINS_Z = 24;