float linearize_reverse_z_inf(float z, float n) {
    return -n / z;
}

float linearize_reverse_z(float z, float n, float f) {
    return f * n / ((n - f) * z - n);
}

float linearize_reverse_z_ortho(float z, float n, float f) {
    return (f - n) * z - f;
}

float project_reverse_z_inf(float z, float n, float f) {
    return -n / z;
}

float project_reverse_z_ortho(float z, float n, float f) {
    return (z + f) / (f - n);
}
