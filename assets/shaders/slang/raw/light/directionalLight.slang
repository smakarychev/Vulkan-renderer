implementing light;

import "core/attributes";

[StandaloneType("directionalLight")]
public struct DirectionalLight {
    [DefaultValue("0.0f, -1.0f, 0.0f")]
    public float3 direction;

    [DefaultValue("1.0f")]
    public float3 color;

    [DefaultValue("1.0f")]
    public float intensity;

    [DefaultValue("1.0f")]
    public float radius;

    public __init(float3 direction, float3 color, float intensity, float radius) {
        this.direction = direction;
        this.color = color;
        this.intensity = intensity;
        this.radius = radius;
    }
}