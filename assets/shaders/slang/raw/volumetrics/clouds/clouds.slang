implementing volumetrics;

import "core/lib";

static const float3 CLOUDS_EARTH_CENTER_DEFAULT = float3(0.0f);
static const float CLOUDS_EARTH_RADIUS_DEFAULT_M = AtmosphereSettings.EARTH_RADIUS_DEFAULT_KM * 1000.0f;
static const float CLOUDS_MIN_RADIUS_DEFAULT_M = CLOUDS_EARTH_RADIUS_DEFAULT_M + 1500.0f;
static const float CLOUDS_MAX_RADIUS_DEFAULT_M = CLOUDS_MIN_RADIUS_DEFAULT_M + 3500.0f;
static const float CLOUDS_DEPTH_MAX = 65504.0f;
static const uint CLOUDS_REPROJECTION_RES = 4;
static const uint CLOUDS_REPROJECTION_RES_2D = 16;
static const uint2 CLOUDS_REPROJECTION_OFFSETS[CLOUDS_REPROJECTION_RES_2D] = {
    uint2(2,1), uint2(1,2), uint2(2,0), uint2(0,1),
    uint2(2,3), uint2(3,2), uint2(3,1), uint2(0,3),
    uint2(1,0), uint2(1,1), uint2(3,3), uint2(0,0),
    uint2(2,2), uint2(1,3), uint2(3,0), uint2(0,2)
};

/// Returns intersection with cloud layer
/// @param viewPosition position of camera on the surface of Earth which is result of `getViewPositionMeters`
/// @param rd view ray direction, normalized
/// @param earthCenter Earth center coordinates in world space in meters
/// @param layerMinMax minimum and maximum radius of cloud layers in meters
public Intersection getCloudLayerIntersection(float3 viewPosition, float3 rd, float3 earthCenter, float2 layerMinMax) {
    float tmin = 0.0f, distance = 0.0f;
    const Intersection maxIntersection = intersectSphere(viewPosition, rd, earthCenter, layerMinMax.y);
    if (maxIntersection.depth == 0) {
        return Intersection(0, 0);
    }
    const Intersection minIntersection = intersectSphere(viewPosition, rd, earthCenter, layerMinMax.x);
    if (minIntersection.depth == 0) {
        tmin = maxIntersection.t;
        distance = maxIntersection.depth;
    }
    else {
        float top = maxIntersection.t > 0.0f ? maxIntersection.t : maxIntersection.t + maxIntersection.depth;
        float bottom = minIntersection.t > 0.0f ? minIntersection.t : minIntersection.t + minIntersection.depth;
        if (minIntersection.t > 0.0)
            top = 0.0f;

        tmin = min(top, bottom);
        distance = max(top, bottom);
        distance = distance - tmin;
    }

    return Intersection(tmin, distance);
}

float remap(float val, float omin, float omax, float nmin, float nmax) {
    return nmin + (val - omin) / (omax - omin) * (nmax - nmin);
}

float remapClamp(float val, float omin, float omax, float nmin, float nmax) {
    return clamp(remap(val, omin, omax, nmin, nmax), nmin, nmax);
}

float remap01(float val, float nmin, float nmax) {
    return saturate((val - nmin) / (nmax - nmin));
}

float3 remap01noclamp(float3 val, float nmin, float nmax) {
    return (val - nmin) / (nmax - nmin);
}

float relativeHeight(float3 p, float3 earthCenter, float2 cloudMinMax) {
    const float toCenter = distance(p, earthCenter);

    return (toCenter - cloudMinMax.x) / (cloudMinMax.y - cloudMinMax.x);
}

float3 encodeCurl(float3 noise) {
    return (noise + 1.0f) * 0.5f;
}

float3 decodeCurl(float3 noise) {
    return (noise - 0.5f) * 2.0f;
}
