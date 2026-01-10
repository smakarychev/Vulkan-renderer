module slimeMold;

import "core/lib";

static const uint WORKGROUP_SIZE = 16;

[SpecializationConstant] const bool SLIME_MAP_STAGE = false;
[SpecializationConstant] const bool SLIME_DIFFUSE_STAGE = false;
[SpecializationConstant] const bool SLIME_GRADIENT_STAGE = false;

struct Traits {
    float moveSpeed;
    float turnSpeed;
    float sensorAngle;
    float sensorOffset;
    float r, g, b;
    float contagionThreshold;
    uint contagionSteps;
};

struct Slime {
    float2 position;
    float angle;
    uint traitsIndex;
    uint contagionSteps;
};

struct GradColors {
    float4 a;
    float4 b;
    float4 c;
    float4 d;
};

struct Resources {
    RWStructuredBuffer<Slime> slime;
    RWTexture2D slimeMap;
    StructuredBuffer<Traits> traits;
    ConstantBuffer<GradColors> colors;
    RWTexture2D diffuseMap;
    RWTexture2D gradientMap;
}

ParameterBlock<Resources> resources;

uint hash(inout uint state) {
    state ^= 2747636419u;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;

    return state;
}

float randomUnit(inout uint state) {
    return hash(state) / 4294967295.0;
}

float sense(Slime slime, float sensorAngleLocal, float sensorOffset, int width, int height) {
    float sensorAngle = slime.angle + sensorAngleLocal;
    float2 sensorDir = float2(cos(sensorAngle), sin(sensorAngle));
    float2 sensorPosition = slime.position + sensorDir * sensorOffset;
    int2 sensorPixel = int2(sensorPosition);

    float sum = 0.0f;

    int radius = 1;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            int sampleX = clamp(sensorPixel.x + x, 0, width - 1);
            int sampleY = clamp(sensorPixel.y + y, 0, height - 1);
            sum += dot(resources.slimeMap[int2(sampleX, sampleY)], float4(1.0f));
        }
    }

    return sum;
}

void updateSlimeMap(
    uint2 dispatchThreadID,
    int width,
    int height,
    uint slimeCount,
    float deltaTime,
    float time) {
    uint id = flatten2d(dispatchThreadID, WORKGROUP_SIZE);
    if (id >= slimeCount)
        return;

    float2 slimePosition = resources.slime[id].position;
    uint randomSeed = id + uint(time);
    randomSeed = uint(slimePosition.x * width + slimePosition.y * height + hash(randomSeed));
    randomSeed = hash(randomSeed);

    Traits traits = resources.traits[resources.slime[id].traitsIndex];
    float sensorAngle = traits.sensorAngle;
    float sensorOffset = traits.sensorOffset;
    float weightForward = sense(resources.slime[id], 0, sensorOffset, width, height);
    float weightLeft = sense(resources.slime[id], sensorAngle, sensorOffset, width, height);
    float weightRight = sense(resources.slime[id], -sensorAngle, sensorOffset, width, height);
    float slimeAngle = resources.slime[id].angle;

    float randomTurnRate = randomUnit(randomSeed);
    float angleDelta = 0.0f;
    if (weightForward > weightLeft && weightForward > weightRight)
        angleDelta += 0.0f;
    else if (weightForward < weightLeft && weightForward < weightRight)
        angleDelta += ((randomTurnRate - 0.5f) * 2.0f) * traits.turnSpeed * deltaTime;
    else if (weightRight > weightLeft)
        angleDelta -= randomTurnRate * traits.turnSpeed * deltaTime;
    else if (weightLeft > weightRight)
        angleDelta += randomTurnRate * traits.turnSpeed * deltaTime;

    if (resources.slime[id].contagionSteps > 0) {
        angleDelta = -angleDelta;
        resources.slime[id].contagionSteps--;
    }

    if (randomUnit(randomSeed) < traits.contagionThreshold) {
        resources.slime[id].contagionSteps = traits.contagionSteps;
    }

    slimeAngle += angleDelta;
    resources.slime[id].angle = slimeAngle;

    float2 velocity = float2(cos(slimeAngle), sin(slimeAngle)) * traits.moveSpeed * deltaTime;
    slimePosition += velocity * deltaTime;

    if (slimePosition.x < 0.0f || slimePosition.x >= width) {
        velocity.x *= -1;
        resources.slime[id].angle = atan2(velocity.y, velocity.x);
        slimePosition.x = clamp(slimePosition.x, 0.0f, width - 1);
        slimePosition.y = clamp(slimePosition.y, 0.0f, height - 1);
    }
    if (slimePosition.y < 0.0f || slimePosition.y >= height) {
        velocity.y *= -1;
        resources.slime[id].angle = atan2(velocity.y, velocity.x);
        slimePosition.x = clamp(slimePosition.x, 0.0f, width - 1);
        slimePosition.y = clamp(slimePosition.y, 0.0f, height - 1);
    }

    resources.slime[id].position = slimePosition;

    int2 coord = int2(slimePosition);

    float4 color = float4(traits.r, traits.g, traits.b, 1.0f);
    resources.slimeMap[coord] = color;
}

void diffuseSlimeMap(
    uint2 dispatchThreadID,
    float width,
    float height,
    float deltaTime,
    float diffuseRate,
    float decayRate) {
    int2 pos = int2(dispatchThreadID);
    if (pos.x >=  uint(width) || pos.y >= uint(height))
        return;

    float4 diffusedColor = float4(0.0f);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            int sampleX = clamp(pos.x + x, 0, int(width) - 1);
            int sampleY = clamp(pos.y + y, 0, int(height) - 1);
            diffusedColor += resources.slimeMap[int2(sampleX, sampleY)];
        }
    }
    diffusedColor /= 9.0f;

    float diffuseWeight = clamp(diffuseRate * deltaTime, 0.0f, 1.0f);
    diffusedColor = lerp(resources.slimeMap[pos], diffusedColor, diffuseWeight);
    diffusedColor = clamp(diffusedColor - decayRate * deltaTime, 0.0f, 1.0f);

    resources.diffuseMap[pos] = diffusedColor;
}

void gradientSlimeMap(uint2 dispatchThreadID, float width, float height) {
    // https://iquilezles.org/articles/palettes/

    int2 pos = int2(dispatchThreadID);
    if (pos.x >=  int(width) || pos.y >= int(height))
        return;

    float4 color = resources.diffuseMap[pos];
    float brightness = color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
    brightness = clamp(brightness, 0.0f, 1.0f);

    GradColors colors = resources.colors;
    color = colors.a +
        colors.b * cos(2.0 * PI * (colors.c * brightness + colors.d));
    color = max(float4(float3(0.0f), 1.0f), color - float4(float3(0.004), 1.0f));
    color = (color * (6.2 * color + .5)) / (color * (6.2 * color + 1.7) + 0.06);
    color.a = 1.0f;
    resources.gradientMap[pos] = color;
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, WORKGROUP_SIZE, 1)]
void slimeMain(
    uint2 dispatchThreadID : SV_DispatchThreadID,
    uniform float width,
    uniform float height,
    uniform uint slimeCount,
    uniform float deltaTime,
    uniform float time,
    uniform float diffuseRate,
    uniform float decayRate) {

    if (SLIME_MAP_STAGE)
        updateSlimeMap(dispatchThreadID, (int)width, (int)height, slimeCount, deltaTime, time);
    else if (SLIME_DIFFUSE_STAGE)
        diffuseSlimeMap(dispatchThreadID, width, height, deltaTime, diffuseRate, decayRate);
    else if (SLIME_GRADIENT_STAGE)
        gradientSlimeMap(dispatchThreadID, width, height);
}