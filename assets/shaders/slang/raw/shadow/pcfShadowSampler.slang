implementing shadow;

#ifndef PCF_FILTER_SIZE
#define PCF_FILTER_SIZE 7
#endif // PCF_FILTER_SIZE

public struct PcfShadowSampler : IShadowSampler {
    float samplePcf(const IShadowCtx ctx, float2 baseUv, float2 deltaPx, float2 shadowSizeInverseerse, float z) {
        return ctx.sample(baseUv + deltaPx * shadowSizeInverseerse, z);
    }

    public float sample(const IShadowCtx ctx, float2 uv, float z) {
        // implementation is almost identical to https://github.com/TheRealMJP/Shadows

        const float2 shadowSize = ctx.shadowMapSize();
        const float2 shadowSizeInverse = 1.0f / shadowSize;
        
        uv = uv.xy * shadowSize;
        float2 baseUv = float2(floor(uv.x + 0.5f), floor(uv.y + 0.5f));
        const float s = uv.x + 0.5f - baseUv.x;
        const float t = uv.y + 0.5f - baseUv.y;

        baseUv -= float2(0.5f, 0.5f);
        baseUv *= shadowSizeInverse;

        float sum = 0;

#if PCF_FILTER_SIZE == 2
        return ctx.sample(uv, z);
#elif PCF_FILTER_SIZE == 3
        const float uw0 = (3 - 2 * s);
        const float uw1 = (1 + 2 * s);
    
        const float u0 = (2 - s) / uw0 - 1;
        const float u1 = s / uw1 + 1;
    
        const float vw0 = (3 - 2 * t);
        const float vw1 = (1 + 2 * t);
    
        const float v0 = (2 - t) / vw0 - 1;
        const float v1 = t / vw1 + 1;
    
        sum += uw0 * vw0 * samplePcf(ctx, baseUv, float2(u0, v0), shadowSizeInverse, z);
        sum += uw1 * vw0 * samplePcf(ctx, baseUv, float2(u1, v0), shadowSizeInverse, z);
        sum += uw0 * vw1 * samplePcf(ctx, baseUv, float2(u0, v1), shadowSizeInverse, z);
        sum += uw1 * vw1 * samplePcf(ctx, baseUv, float2(u1, v1), shadowSizeInverse, z);

        return sum * 1.0f / 16;
#elif PCF_FILTER_SIZE == 5
        const float uw0 = (4 - 3 * s);
        const float uw1 = 7;
        const float uw2 = (1 + 3 * s);
    
        const float u0 = (3 - 2 * s) / uw0 - 2;
        const float u1 = (3 + s) / uw1;
        const float u2 = s / uw2 + 2;
    
        const float vw0 = (4 - 3 * t);
        const float vw1 = 7;
        const float vw2 = (1 + 3 * t);
    
        const float v0 = (3 - 2 * t) / vw0 - 2;
        const float v1 = (3 + t) / vw1;
        const float v2 = t / vw2 + 2;
    
        sum += uw0 * vw0 * samplePcf(ctx, baseUv, float2(u0, v0), shadowSizeInverse, z);
        sum += uw1 * vw0 * samplePcf(ctx, baseUv, float2(u1, v0), shadowSizeInverse, z);
        sum += uw2 * vw0 * samplePcf(ctx, baseUv, float2(u2, v0), shadowSizeInverse, z);
        
        sum += uw0 * vw1 * samplePcf(ctx, baseUv, float2(u0, v1), shadowSizeInverse, z);
        sum += uw1 * vw1 * samplePcf(ctx, baseUv, float2(u1, v1), shadowSizeInverse, z);
        sum += uw2 * vw1 * samplePcf(ctx, baseUv, float2(u2, v1), shadowSizeInverse, z);

        sum += uw0 * vw2 * samplePcf(ctx, baseUv, float2(u0, v2), shadowSizeInverse, z);
        sum += uw1 * vw2 * samplePcf(ctx, baseUv, float2(u1, v2), shadowSizeInverse, z);
        sum += uw2 * vw2 * samplePcf(ctx, baseUv, float2(u2, v2), shadowSizeInverse, z);
    
        return sum * 1.0f / 144;
#else // PCF_FILTER_SIZE == 7
        const float uw0 = (5 * s - 6);
        const float uw1 = (11 * s - 28);
        const float uw2 = -(11 * s + 17);
        const float uw3 = -(5 * s + 1);
    
        const float u0 = (4 * s - 5) / uw0 - 3;
        const float u1 = (4 * s - 16) / uw1 - 1;
        const float u2 = -(7 * s + 5) / uw2 + 1;
        const float u3 = -s / uw3 + 3;
    
        const float vw0 = (5 * t - 6);
        const float vw1 = (11 * t - 28);
        const float vw2 = -(11 * t + 17);
        const float vw3 = -(5 * t + 1);
    
        const float v0 = (4 * t - 5) / vw0 - 3;
        const float v1 = (4 * t - 16) / vw1 - 1;
        const float v2 = -(7 * t + 5) / vw2 + 1;
        const float v3 = -t / vw3 + 3;
    
        sum += uw0 * vw0 * samplePcf(ctx, baseUv, float2(u0, v0), shadowSizeInverse, z);
        sum += uw1 * vw0 * samplePcf(ctx, baseUv, float2(u1, v0), shadowSizeInverse, z);
        sum += uw2 * vw0 * samplePcf(ctx, baseUv, float2(u2, v0), shadowSizeInverse, z);
        sum += uw3 * vw0 * samplePcf(ctx, baseUv, float2(u3, v0), shadowSizeInverse, z);
    
        sum += uw0 * vw1 * samplePcf(ctx, baseUv, float2(u0, v1), shadowSizeInverse, z);
        sum += uw1 * vw1 * samplePcf(ctx, baseUv, float2(u1, v1), shadowSizeInverse, z);
        sum += uw2 * vw1 * samplePcf(ctx, baseUv, float2(u2, v1), shadowSizeInverse, z);
        sum += uw3 * vw1 * samplePcf(ctx, baseUv, float2(u3, v1), shadowSizeInverse, z);
    
        sum += uw0 * vw2 * samplePcf(ctx, baseUv, float2(u0, v2), shadowSizeInverse, z);
        sum += uw1 * vw2 * samplePcf(ctx, baseUv, float2(u1, v2), shadowSizeInverse, z);
        sum += uw2 * vw2 * samplePcf(ctx, baseUv, float2(u2, v2), shadowSizeInverse, z);
        sum += uw3 * vw2 * samplePcf(ctx, baseUv, float2(u3, v2), shadowSizeInverse, z);
    
        sum += uw0 * vw3 * samplePcf(ctx, baseUv, float2(u0, v3), shadowSizeInverse, z);
        sum += uw1 * vw3 * samplePcf(ctx, baseUv, float2(u1, v3), shadowSizeInverse, z);
        sum += uw2 * vw3 * samplePcf(ctx, baseUv, float2(u2, v3), shadowSizeInverse, z);
        sum += uw3 * vw3 * samplePcf(ctx, baseUv, float2(u3, v3), shadowSizeInverse, z);
    
        return sum * 1.0f / 2704;
#endif
    }
}