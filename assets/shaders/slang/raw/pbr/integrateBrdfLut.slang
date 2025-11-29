module integrateBrdfLut;

import "core/lib";
import "pbr";

struct Resources {
    RWTexture2D<float2> brdf;
}

ParameterBlock<Resources> resources;

float2 integrateBrdf(float NoV, float roughness) {
    float3 V;
    V.x = sqrt(1.0f - NoV * NoV);
    V.y = 0.0f;
    V.z = NoV;

    float A = 0.0f;
    float B = 0.0f;

    const float3 normal = float3(0.0f, 0.0f, 1.0f);

    const uint n = 1024u;
    for(uint i = 0u; i < n; i++)
    {
        const float2 hammersley = hammersley2d(i, n);
        const float3 sampleDir = importanceGgxHemisphereSample(hammersley, normal, roughness);
        const float3 L = normalize(2.0f * dot(V, sampleDir) * sampleDir - V);

        const float NoL = saturate(L.z);
        const float NoH = saturate(sampleDir.z);
        const float VoH = saturate(dot(V, sampleDir));

        if (NoL > 0.0) {
            const float G = ggxGSmithHeightCorrelated(NoV, NoL, roughness);
            const float GV = (G * VoH) / (NoH * NoV);
            const float Fc = pow(1.0f - VoH, 5.0f);

            A += (1.0f - Fc) * GV;
            B += Fc * GV;
        }
    }
    A /= float(n);
    B /= float(n);
    
    return float2(A, B);
}

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(uint2 dispatchThreadID: SV_DispatchThreadID, uniform float2 brdfSizeInverse) {
    const float2 uv = (float2(dispatchThreadID) + 0.5f) * brdfSizeInverse;
    resources.brdf[dispatchThreadID] = integrateBrdf(uv.x, uv.y);
}