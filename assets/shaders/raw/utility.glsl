float linearize_reverse_z(float z, float n, float f) {
    return f * n / ((n - f) * z - n);
}