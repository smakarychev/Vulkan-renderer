implementing volumetrics;

struct TransmittanceLutResources {
    RWTexture2D<float4> lut;
    ConstantBuffer<ViewInfo> view;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void transmittanceLut(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<TransmittanceLutResources> resources) {

    if (dispatchThreadID.x >= TRANSMITTANCE_LUT_WIDTH || dispatchThreadID.y >= TRANSMITTANCE_LUT_HEIGHT)
        return;

    const float2 uv = (float2(dispatchThreadID) + 0.5f) / float2(TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT);
    const float2 rMu = transmittanceRMuFromUv(resources.view.atmosphere, uv);

    const float3 viewPos = float3(0.0f, rMu.x, 0.0f);
    const float3 viewDir = float3(0.0f, rMu.y, sqrt(1.0f - rMu.y * rMu.y));

    const float d = distanceToAtmosphereTop(resources.view.atmosphere, rMu.x, rMu.y);

    // e^(-integral(extinction(x) * dx))
    float3 totalExtinction = float3(0.0f);
    const float stepSize = d / TRANSMITTANCE_STEPS;
    for (float i = 0.0f; i < TRANSMITTANCE_STEPS; i += 1.0f) {
        const float3 x = viewPos + viewDir * stepSize * i;
        const float3 extinction = AtmosphereMedia.sampleAt(x, resources.view.atmosphere).extinction;
        totalExtinction += extinction;
    }

    resources.lut[dispatchThreadID] = float4(exp(-totalExtinction * stepSize), 1.0f);
}

struct TransmittanceAtViewSamplers {
    [ImmutableSampler(SamplerFlags.Linear | SamplerFlags.ClampEdge)]
    SamplerState sampler;
}

struct TransmittanceAtViewResources {
    Texture2D<float4> transmittanceLut;
    RWStructuredBuffer<ViewInfo> view;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void transmittanceLutAtView(
    uniform ParameterBlock<TransmittanceAtViewSamplers> samplers,
    uniform ParameterBlock<TransmittanceAtViewResources> resources) {
    
    const float3 pos = getViewPosition(resources.view[0].camera.position, resources.view[0].atmosphere.surface);
    const float3 sunDir = -resources.view[0].shading.primaryDirectionalLightDirection;
    const float r = length(pos);
    const float3 up = pos / r;
    
    const float surfaceShadow = 
        getVisibility(resources.view[0].atmosphere, pos, sunDir, float3(0.0f) + PLANET_RADIUS_OFFSET_KM * up);
    
    float3 transmittance = float3(0.0f);
    const float mu = dot(up, sunDir);
    const float2 transmittanceUv = transmittanceUvFromRMu(resources.view[0].atmosphere, r, dot(up, sunDir));
    transmittance = surfaceShadow * resources.transmittanceLut.SampleLevel(samplers.sampler, transmittanceUv, 0.0f).rgb;
    
    resources.view[0].shading.atmosphereTransmittanceAtView = float4(transmittance, 1.0f);
}

struct MultiscatteringLutSamplers {
    [ImmutableSampler(SamplerFlags.Linear | SamplerFlags.ClampEdge)]
    SamplerState sampler;
}

struct MultiscatteringLutResources {
    Texture2D<float4> transmittanceLut;
    RWTexture2D<float4> multiscatteringLut;
    ConstantBuffer<ViewInfo> view;
}

groupshared float3 sMultiscattering[2];
groupshared float3 sL[2];

[shader("compute")]
[numthreads(1, 1, MULTISCATTERING_SPHERE_SAMPLES_INT)]
void multiscatteringLut(
    uint3 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<MultiscatteringLutSamplers> samplers,
    uniform ParameterBlock<MultiscatteringLutResources> resources) {

    if (
        dispatchThreadID.x >= MULTISCATTERING_LUT_RES || 
        dispatchThreadID.y >= MULTISCATTERING_LUT_RES || 
        dispatchThreadID.z >= MULTISCATTERING_SPHERE_SAMPLES)
        return;

    const float2 uv = subUvToUnit((float2(dispatchThreadID.xy) + 0.5f) / float2(MULTISCATTERING_LUT_RES), 
        float2(MULTISCATTERING_LUT_RES));

    const AtmosphereSettings atmosphere = resources.view.atmosphere;
    
    const float mu = uv.x * 2.0f - 1.0f;
    const float3 sunDir = normalize(float3(0.0f, mu, sqrt(1.0f - mu * mu)));
    
    const float r = atmosphere.surface +
        clamp(uv.y + PLANET_RADIUS_OFFSET_UV, 0.0f, 1.0f) * 
        (atmosphere.atmosphere - atmosphere.surface - PLANET_RADIUS_OFFSET_UV);
    const float3 ro = float3(0.0f, r, 0.0f);
    float3 rd;

    const float sphereSolidAngle = 4.0f * PI;
    const float uniformPhase = 1.0f / sphereSolidAngle;
    const float samplesSqrt = sqrt(MULTISCATTERING_SPHERE_SAMPLES);

    float3 multiscattering;
    float3 L;
    {
        const float i = 0.5f + float(dispatchThreadID.z / samplesSqrt);
        const float j = 0.5f + float(dispatchThreadID.z - float((dispatchThreadID.z / samplesSqrt) * samplesSqrt));
        const float randA = i / samplesSqrt;
        const float randB = j / samplesSqrt;
        const float theta = 2.0f * PI * randA;
        const float phi = acos(1.0f - 2.0f * randB);	
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        rd.x = cosTheta * sinPhi;
        rd.y = cosPhi;
        rd.z = sinTheta * sinPhi;

        AmosphereScatteredLuminanceInfo scatteringInfo;
        scatteringInfo.linearSampler = samplers.sampler;
        scatteringInfo.transmittanceLut = resources.transmittanceLut;
        scatteringInfo.view = resources.view;
        scatteringInfo.globalL = float3(1.0f);
        scatteringInfo.sunDir = sunDir;
        scatteringInfo.maxDepth = MAX_DEPTH;
        scatteringInfo.sampleCount = MULTISCATTERING_STEPS;

        const AmosphereScatteredLuminance scattering = AmosphereScatteredLuminance.integrate<
            /*withShadow*/ false,
            /*withMultiscattering*/ false,
            /*withSurfaceReflection*/ true,
            /*useUniformPhase*/ true>(scatteringInfo, ro, rd);

        const float3 waveMultiscattering = 
            uniformPhase *
            WaveActiveSum(scattering.multiscattering * sphereSolidAngle / float(MULTISCATTERING_SPHERE_SAMPLES));

        const float3 waveL = 
            uniformPhase *
            WaveActiveSum(scattering.luminance * sphereSolidAngle / float(MULTISCATTERING_SPHERE_SAMPLES));
            
        if (WaveGetLaneCount() == 64) {
            multiscattering = waveMultiscattering;
            L = waveL;
        } else if (WaveGetLaneCount() == 32) {
            if (WaveIsFirstLane()) {
                /*uint waveIndex =  dispatchThreadID / 32; */
                uint waveIndex = dispatchThreadID.z >> 5;
                sMultiscattering[waveIndex] = waveMultiscattering;
                sL[waveIndex] = waveL;
            }
            GroupMemoryBarrierWithGroupSync();
            multiscattering = sMultiscattering[0] + sMultiscattering[1];
            L = sL[0] + sL[1];
        }
    }
    
    if (dispatchThreadID.z != 0)
        return;

    const float3 MSR = multiscattering;
    const float3 geomSum = 1.0f / (1.0f - MSR);
    L = L * geomSum;

    resources.multiscatteringLut[dispatchThreadID.xy] = float4(L, 1.0f);
}