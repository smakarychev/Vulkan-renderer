implementing shadow;

float3 shadowOffsetNormal(float2 mapSize, float3 normal, float3 lightDirection) {
    const float texelSize = 2.0f / mapSize.x;
    const float normalOffsetScale = saturate(1.0f - dot(normal, lightDirection));
    
    return texelSize * normalOffsetScale * normal;
}

float shadowOffsetConstant() {
    return 5e-3f;
}