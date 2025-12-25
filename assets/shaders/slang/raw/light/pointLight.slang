implementing light;

import "core/attributes";

[StandaloneType("pointLight")]
public struct PointLight {
    [DefaultValue("0.0f")]
    public float3 position;

    [DefaultValue("1.0f")]
    public float3 color;

    [DefaultValue("1.0f")]
    public float intensity;

    [DefaultValue("1.0f")]
    public float radius;

    public __init(float3 position, float3 color, float intensity, float radius) {
        this.position = position;
        this.color = color;
        this.intensity = intensity;
        this.radius = radius;
    }
}