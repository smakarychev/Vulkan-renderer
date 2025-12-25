implementing light;

import "core/lib";

/// Represents second order spherical harmonics
public struct SH2 {
    /// 9 coefficients of the second order spherical harmonics
    float16_t lm[5 + 3 + 1];

    /// Create empty SH2
    public static SH2 zero() {
        SH2 sh;
        [unroll]
        for (uint i = 0; i < 5 + 3 + 1; i++)
            sh.lm[i] = 0;

        return sh;
    }

    /// Evaluate SH2 coefficients for a given direction
    /// @param direction evaluation direction
    public static SH2 evaluate(float16_t3 direction) {
        SH2 sh;
        sh.lm[0] = float16_t( 0.282095f);
        sh.lm[1] = float16_t(-0.488603f * direction.y);
        sh.lm[2] = float16_t( 0.488603f * direction.z);
        sh.lm[3] = float16_t(-0.488603f * direction.x);

        const float16_t3 dir2 = direction * direction;
        sh.lm[4] = float16_t( 1.092548f *  direction.x  * direction.y);
        sh.lm[5] = float16_t(-1.092548f *  direction.y  * direction.z);
        sh.lm[6] = float16_t( 0.315392f * (3.0f   * dir2.z - 1.0f));
        sh.lm[7] = float16_t(-1.092548f *  direction.x  * direction.z);
        sh.lm[8] = float16_t( 0.546274f * (dir2.x - dir2.y));
        
        return sh;
    }

    /// Evaluate SH2 prenormalized irradiance coefficients for a given direction
    /// @param direction evaluation direction
    public static SH2 evaluateIrradiancePrenormalized(float16_t3 direction) {
        // E_lm = Ah_l * L_lm = Ah_l * K_lm * <4pi/N * sum(L * Yh_lm)>
        // E = sum(E_lm * Y_lm) = sum(Ah_l * K_lm^2 * <4pi/N * sum(L * Yh_lm)> * Yh_lm)
        // we are going to store <4pi/N * sum(L * Ah_l * K_lm^2 * Yh_lm)> = <4pi/N * sum(L * c_i * Yh_lm)>, 
        // Ah_l = (pi, 2pi/3, pi/4) (see https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf)
        // it also gives the nicest coefficients
        // after the whole SH2 is gathered, we are going to rearrange the terms
        // (see last page of https://www.ppsloan.org/publications/StupidSH36.pdf)
            
        const float16_t c0 = float16_t(0.25f); // (1/(2*sqrt(pi))^2 * pi
        const float16_t c1 = float16_t(0.5f); // ((1/2)*sqrt(3/pi))^2 * 2pi/3
        const float16_t c2 = float16_t(0.9375f); // ((1/2)*sqrt(15/pi))^2 * pi/4
        const float16_t c3 = float16_t(0.078125f); // ((1/4)*sqrt(5/pi))^2 * pi/4
        const float16_t c4 = float16_t(0.234375f); // ((1/4)*sqrt(15/pi))^2 * pi/4

        SH2 sh;
        sh.lm[0] = c0;
        
        sh.lm[1] = c1 * direction.y;
        sh.lm[2] = c1 * direction.z;
        sh.lm[3] = c1 * direction.x;

        const float16_t3 dir2 = direction * direction;
        sh.lm[4] = c2 * direction.x * direction.y;
        sh.lm[5] = c2 * direction.y * direction.z;
        sh.lm[6] = c3 * float16_t(3.0f * dir2.z - 1.0f);
        sh.lm[7] = c2 * float16_t(direction.x * direction.z);
        sh.lm[8] = c4 * float16_t(dir2.x - dir2.y);
        
        return sh;
    }

    /// Multiply SH2 by scalar value
    /// @param value multiplier
    public SH2 multiply(float16_t value) {
        SH2 sh;
        [unroll]
        for (uint i = 0; i < 5 + 3 + 1; i++)
            sh.lm[i] = lm[i] * value;
        
        return sh;
    }

    /// Add two SH2
    /// @param sh2 SH2 to be added
    public SH2 add(const SH2 sh2) {
        SH2 sh;
        [unroll]
        for (uint i = 0; i < 5 + 3 + 1; i++)
            sh.lm[i] = lm[i] + sh2.lm[i];
        
        return sh;
    }

    /// Multiply SH2 by rgb color value
    /// @param color multiplier color value
    /// @returns SH2RGB struct
    public SH2RGB multiplyRGB(float16_t3 color) {
        SH2RGB sh;
        sh.r = multiply(color.r);
        sh.g = multiply(color.g);
        sh.b = multiply(color.b);
        
        return sh;
    }
}

/// Represents second order spherical harmonics storing rgb color
public struct SH2RGB {
    SH2 r;
    SH2 g;
    SH2 b;

    /// Constructs SH2RGB by multiplying SH2 with color
    public __init(const SH2 sh, float16_t3 color) {
        r = sh.multiply(color.r);
        g = sh.multiply(color.g);
        b = sh.multiply(color.b);
    }

    /// Create empty SH2RGB
    public static SH2RGB zero() {
        SH2RGB sh;
        sh.r = SH2.zero();
        sh.g = SH2.zero();
        sh.b = SH2.zero();

        return sh;
    }

    /// Add two SH2RGB
    /// @param sh2 SH2RGB to be added
    public SH2RGB add(const SH2RGB sh2) {
        SH2RGB sh;
        sh.r = r.add(sh2.r);
        sh.g = g.add(sh2.g);
        sh.b = b.add(sh2.b);

        return sh;
    }
};

/// Represents second order spherical harmonics storing irradiance
public struct SH2Irradiance {
    float4 ar;
    float4 ag;
    float4 ab;
    float4 br;
    float4 bg;
    float4 bb;
    float4 c;

    /// Constructs SH2Irradiance from SH2RGB
    public __init(const SH2RGB sh) {
        ar.x = PI_INVERSE * float(sh.r.lm[3]);
        ar.y = PI_INVERSE * float(sh.r.lm[1]);
        ar.z = PI_INVERSE * float(sh.r.lm[2]);
        ar.w = PI_INVERSE * float(sh.r.lm[0] - sh.r.lm[6]);
        ag.x = PI_INVERSE * float(sh.g.lm[3]);
        ag.y = PI_INVERSE * float(sh.g.lm[1]);
        ag.z = PI_INVERSE * float(sh.g.lm[2]);
        ag.w = PI_INVERSE * float(sh.g.lm[0] - sh.g.lm[6]);
        ab.x = PI_INVERSE * float(sh.b.lm[3]);
        ab.y = PI_INVERSE * float(sh.b.lm[1]);
        ab.z = PI_INVERSE * float(sh.b.lm[2]);
        ab.w = PI_INVERSE * float(sh.b.lm[0] - sh.b.lm[6]);
        
        
        br.x = PI_INVERSE * float(sh.r.lm[4]);
        br.y = PI_INVERSE * float(sh.r.lm[5]);
        br.z = PI_INVERSE * float(sh.r.lm[6]) * 3.0f;
        br.w = PI_INVERSE * float(sh.r.lm[7]);
        bg.x = PI_INVERSE * float(sh.g.lm[4]);
        bg.y = PI_INVERSE * float(sh.g.lm[5]);
        bg.z = PI_INVERSE * float(sh.g.lm[6]) * 3.0f;
        bg.w = PI_INVERSE * float(sh.g.lm[7]);
        bb.x = PI_INVERSE * float(sh.b.lm[4]);
        bb.y = PI_INVERSE * float(sh.b.lm[5]);
        bb.z = PI_INVERSE * float(sh.b.lm[6]) * 3.0f;
        bb.w = PI_INVERSE * float(sh.b.lm[7]);

        c.x = PI_INVERSE * float(sh.r.lm[8]);
        c.y = PI_INVERSE * float(sh.g.lm[8]);
        c.z = PI_INVERSE * float(sh.b.lm[8]);
        c.w = 1.0f;
    }

    /// Get color for a given direction
    /// @param direction shading direction
    /// @returns RGB Color
    public float3 shade(float3 direction) {
        float3 a;
        a.r = dot(ar, float4(direction, 1.0f));
        a.g = dot(ag, float4(direction, 1.0f));
        a.b = dot(ab, float4(direction, 1.0f));
        
        float3 b;
        float4 dir2 = direction.xyzz * direction.yzzx;
        b.r = dot(br, dir2);
        b.g = dot(bg, dir2);
        b.b = dot(bb, dir2);
        
        const float3 c = c.rgb * (direction.x * direction.x - direction.y * direction.y);
        
        return a + b + c;
    }

    /// Get constant color component of irradiance
    /// @returns RGB Color
    public float3 shadeConstant() {
        return float3(ar.w, ag.w, ab.w);
    }
};