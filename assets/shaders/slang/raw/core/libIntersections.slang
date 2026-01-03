implementing lib;

/// Represents first ray-object intersection
public struct Intersection {
    public bool missed() { return depth == 0; }
    public __init(float t_, float depth_) { t = t_; depth = depth_;}
	public float t;
    public float depth;
}

/// Finds the intersecion between ray and sphere
/// @param ro origin of the ray
/// @param rd normalized direction of the ray
/// @param origin coordinate of the sphere origin
/// @param radius sphere radius
/// @return The `Intersection` object representing intersection between ray and sphere
public Intersection intersectSphere(float3 ro, float3 rd, float3 origin, float radius) {
	const float3 oc = ro - origin;
    const float b = dot(oc, rd);
    const float c = dot(oc, oc) - radius * radius;
    const float t = b * b - c;
    // no hit
    if (t < 0.0f)
        return Intersection(0, 0);

    const float tsqrt = sqrt(t);
    if (-b < -tsqrt)
        return Intersection(0, 0);

    if (-b > tsqrt)
        return Intersection(-b - tsqrt, 2.0f * tsqrt);

    return Intersection(0.0f, -b + tsqrt);
}