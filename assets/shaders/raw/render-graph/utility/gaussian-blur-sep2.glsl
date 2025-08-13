#ifndef BLUR_DATA_TYPE
#define BLUR_DATA_TYPE vec4
#endif // BLUR_DATA_TYPE

#ifndef BLUR_IMAGE_SIZE
#error "Missing required macro BLUR_IMAGE_SIZE: #define BLUR_IMAGE_SIZE textureSize(image, 0)"
#endif // BLUR_DATA_TYPE


#ifndef BLUR_IMAGE_LOAD
#error "Missing required macro BLUR_IMAGE_LOAD(uv): #define BLUR_IMAGE_LOAD(uv) textureLod(sampler2D(image, sampler), uv, 0)"
#endif // BLUR_IMAGE_LOAD

#ifndef BLUR_IMAGE_STORE
#error "Missing required macro BLUR_IMAGE_STORE(coord, color): #define BLUR_IMAGE_STORE(coord, color) imageStore(image, coord, vec4(color))"
#endif // BLUR_IMAGE_STORE

#ifndef BLUR_WORKGROUP_SIZE
#define BLUR_WORKGROUP_SIZE 8
#endif // BLUR_WORKGROUP_SIZE

#extension GL_EXT_samplerless_texture_functions: require

layout(local_size_x = BLUR_WORKGROUP_SIZE, local_size_y = BLUR_WORKGROUP_SIZE) in;

BLUR_DATA_TYPE gaussian_blur_5(vec2 uv, vec2 image_size_inverse) {
    BLUR_DATA_TYPE blurred = BLUR_DATA_TYPE(0.0f);

#if VERTICAL
    const vec2 offset = vec2(0.0f, 1.3333333333333333f);
#else // VERTICAL
    const vec2 offset = vec2(1.3333333333333333f, 0.0f);
#endif // VERTICAL
    
    blurred += BLUR_IMAGE_LOAD(uv) * 0.29411764705882354f;
    blurred += BLUR_IMAGE_LOAD(uv + offset * image_size_inverse) * 0.35294117647058826f;
    blurred += BLUR_IMAGE_LOAD(uv - offset * image_size_inverse) * 0.35294117647058826f;
    
    return blurred;
}

void main() {
    const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 image_size_inverse = 1.0f / BLUR_IMAGE_SIZE;
    const vec2 uv = vec2(coord + 0.5f) * image_size_inverse;
    
    const BLUR_DATA_TYPE blurred = gaussian_blur_5(uv, image_size_inverse);
    BLUR_IMAGE_STORE(coord, blurred);
}