module ssaoBlur;

import "core/attributes";
import "utility/gaussianBlurSep";

#ifndef VERTICAL
#define VERTICAL 1
#endif // VERTICAL

#if VERTICAL
#define BLUR_DIRECTION GaussianBlurDirection.Vertical
#else  // VERTICAL
#define BLUR_DIRECTION GaussianBlurDirection.Horizontal
#endif // VERTICAL

struct SsaoBlurSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct SsaoBlurResources {
    Texture2D ssao;
    RWTexture2D<float> ssaoBlurred;
}

ParameterBlock<SsaoBlurSamplers> samplers;
ParameterBlock<SsaoBlurResources> resources;

struct SsaoBlur : IGaussianBlur<float> {
    float readImage(float2 uv, float scale) {
        return resources.ssao.SampleLevel(samplers.sampler, uv, 0).r * scale;
    }
    void storeImage(uint2 coord, float val) {
        resources.ssaoBlurred[coord] = val;
    }
    float2 imageSize() {
        float2 size;
        resources.ssao.GetDimensions(size.x, size.y);

        return size;
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
public void main(uint2 dispatchThreadID: SV_DispatchThreadID) {
    SsaoBlur ssaoBlur;
    gaussianBlur.blur5<BLUR_DIRECTION>(ssaoBlur, dispatchThreadID);
}
