implementing volumetrics;

import "shadow/shadow";

public struct AmosphereScatteredLuminanceInfo {
    public SamplerState linearSampler;
    public SamplerComparisonState shadowSampler;
    public Texture2D transmittanceLut;
    public Texture2D multiscatteringLut;
    public CsmResources csm;

    public ConstantBuffer<ViewInfo> view;
    public float3 globalL;
    public float3 sunDir;
    public float maxDepth;
    public float sampleCount;
}

internal struct AmosphereScatteredLuminance {
    static internal AmosphereScatteredLuminance integrate<
        let withShadow : bool,
        let withMultiscattering : bool,
        let withSurfaceReflection : bool,
        let useUniformPhase : bool>(
            const AmosphereScatteredLuminanceInfo info, float3 ro, float3 rd) {
        AmosphereScatteredLuminance scattering = AmosphereScatteredLuminance();
        const Intersection atmosphereIntersection = intersectSphere(ro, rd, float3(0.0f),
            info.view.atmosphere.atmosphere);
        const Intersection surfaceIntersection = intersectSphere(ro, rd, float3(0.0f), info.view.atmosphere.surface);
        
        float depth = atmosphereIntersection.depth;
        const float depthToSurface = surfaceIntersection.t - atmosphereIntersection.t;
        if (surfaceIntersection.depth == 0.0f) {
            if (atmosphereIntersection.depth == 0.0f)
                return scattering;
        } else {
            depth = min(depth, depthToSurface);
        }
        depth = min(depth, info.maxDepth);

        float3 luminance = float3(0.0f);
        float3 opticalDepth = float3(0.0f);
        float3 throughput = float3(1.0f);

        const float cosTheta = dot(rd, info.sunDir);
        const float rayleigh = rayleighPhase(cosTheta);
        const float mie = mieCSPhase(cosTheta);
        const float uniformPhase = 1.0f / (4.0f * PI);
        const float sampleT = 0.3f;

        float t = 0.0f;
        for (float i = 0.0f; i < info.sampleCount; i += 1.0f) {
            const float newT = depth * (i + sampleT) / info.sampleCount;
            const float dt = newT - t;
            t = newT;
            const float3 x = ro + rd * t;
            const AtmosphereMedia media = AtmosphereMedia.sampleAt(x, info.view.atmosphere);

            const float3 sampleOpticalDepth = media.extinction * dt;
            const float3 sampleTransmittance = exp(-sampleOpticalDepth);
            opticalDepth += sampleOpticalDepth;

            const float r = length(x);
            const float3 up = x / r;
            const float mu = dot(info.sunDir, up);
            const float2 transmittanceUv = transmittanceUvFromRMu(info.view.atmosphere, r, mu);
            const float3 transmittance = 
                info.transmittanceLut.SampleLevel(info.linearSampler, transmittanceUv, 0).rgb;
            
            float3 phaseScattering;
            if (useUniformPhase)
                phaseScattering = (media.rayleigh + media.mie) * uniformPhase;
            else
                phaseScattering = media.rayleigh * rayleigh + media.mie * mie;

            const float surfaceShadow = 
                getVisibility(info.view.atmosphere, x, info.sunDir, float3(0.0f) + PLANET_RADIUS_OFFSET_KM * up);

            float shadowMap = 1.0f;
            if (withShadow) {
                const float3 shadowWs = info.view.camera.position + rd * t * 1e+3;
                const float shadowZView = mul(float4(shadowWs, 1.0f), info.view.camera.view).z;

                PcfShadowSampler pcfShadowSampler;
                CsmShadow csmShadow = CsmShadow(info.csm.csmData, info.csm.csmTexture, info.shadowSampler, info.sunDir);
                const float shadow = csmShadow.sample(pcfShadowSampler, shadowWs, float3(0.0f), shadowZView);
                shadowMap = 1 - shadow;

                // todo: add cloud shadows here:
            }

            float3 multiscatteringLuminance = float3(0.0f);
            if (withMultiscattering) {
                const float2 multiscatteringUv = multiscatteringUvFromRMu(info.view.atmosphere, r, mu);
                multiscatteringLuminance = 
                    info.multiscatteringLut.SampleLevel(info.linearSampler, multiscatteringUv, 0).rgb;
            }

            const float3 MS = media.rayleigh + media.mie;
            const float3 MSIntegral = (MS - MS * sampleTransmittance) / media.extinction;
            scattering.multiscattering += throughput * MSIntegral;
            
            const float3 S = info.globalL * (
                shadowMap * surfaceShadow * phaseScattering * transmittance +
                multiscatteringLuminance * MS);
            const float3 SIntegral = (S - S * sampleTransmittance) / media.extinction;
            luminance += throughput * SIntegral;

            throughput *= sampleTransmittance;
        }

        if (withSurfaceReflection && depth == depthToSurface) {
            const float3 x = ro + rd * depth;
            const float r = length(x);
            const float3 up = x / r;
            const float mu = dot(up, info.sunDir);
            const float2 transmittanceUv = transmittanceUvFromRMu(info.view.atmosphere, r, mu);
            const float3 transmittance = 
                info.transmittanceLut.SampleLevel(info.linearSampler, transmittanceUv, 0).rgb;

            const float NoL = saturate(dot(up, info.sunDir));
            luminance += info.globalL * NoL * transmittance * throughput * info.view.atmosphere.surfaceAlbedo.rgb / PI;
        }

        scattering.luminance = luminance;
        scattering.opticalDepth = opticalDepth;
        scattering.transmittance = throughput;

        return scattering;
    }

    internal float3 luminance = float3(0.0f);
    internal float3 opticalDepth = float3(0.0f);
    internal float3 transmittance = float3(0.0f);
    internal float3 multiscattering = float3(0.0f);
}
