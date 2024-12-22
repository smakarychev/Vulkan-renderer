#extension GL_EXT_shader_explicit_arithmetic_types_float16: require 
#extension GL_EXT_control_flow_attributes: require 

struct SH9 {
    float16_t LM[5 + 3 + 1];
};

struct SH9RGB {
    SH9 R;
    SH9 G;
    SH9 B;
};

struct SH9Irradiance {
    vec4 AR;
    vec4 AG;
    vec4 AB;
    vec4 BR;
    vec4 BG;
    vec4 BB;
    vec4 C;
};

SH9 SH_evaluate(f16vec3 dir) {
    SH9 result;
    result.LM[0] = float16_t( 0.282095f);
    result.LM[1] = float16_t(-0.488603f * dir.y);
    result.LM[2] = float16_t( 0.488603f * dir.z);
    result.LM[3] = float16_t(-0.488603f * dir.x);

    const vec3 dir2 = dir * dir;
    result.LM[4] = float16_t( 1.092548f *  dir.x  * dir.y);
    result.LM[5] = float16_t(-1.092548f *  dir.y  * dir.z);
    result.LM[6] = float16_t( 0.315392f * (3.0f   * dir2.z - 1.0f));
    result.LM[7] = float16_t(-1.092548f *  dir.x  * dir.z);
    result.LM[8] = float16_t( 0.546274f * (dir2.x - dir2.y));
    
    return result;
}

vec3 SH_irradiance_shade(SH9Irradiance irradiance, vec3 dir) {
    vec3 a;
    a.r = dot(irradiance.AR, vec4(dir, 1.0f));
    a.g = dot(irradiance.AG, vec4(dir, 1.0f));
    a.b = dot(irradiance.AB, vec4(dir, 1.0f));
    
    vec3 b;
    vec4 dir2 = dir.xyzz * dir.yzzx;
    b.r = dot(irradiance.BR, dir2);
    b.g = dot(irradiance.BG, dir2);
    b.b = dot(irradiance.BB, dir2);
    
    const vec3 c = irradiance.C.rgb * (dir.x * dir.x - dir.y * dir.y);
    
    return a + b + c;
}

SH9 SH_evaluate_irradiance_prenormalized(f16vec3 dir) {
    // E_lm = Ah_l * L_lm = Ah_l * K_lm * <4pi/N * sum(L * Yh_lm)>
    // E = sum(E_lm * Y_lm) = sum(Ah_l * K_lm^2 * <4pi/N * sum(L * Yh_lm)> * Yh_lm)
    // we are going to store <4pi/N * sum(L * Ah_l * K_lm^2 * Yh_lm)> = <4pi/N * sum(L * c_i * Yh_lm)>, 
    // Ah_l = (pi, 2pi/3, pi/4) (see https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf)
    // it also gives the nicest coefficients
    // after the whole SH9 is gathered, we are going to rearrange the terms
    // (see last page of https://www.ppsloan.org/publications/StupidSH36.pdf)
        
    const float16_t c0 = float16_t(0.25f); // (1/(2*sqrt(pi))^2 * pi
    const float16_t c1 = float16_t(0.5f); // ((1/2)*sqrt(3/pi))^2 * 2pi/3
    const float16_t c2 = float16_t(0.9375f); // ((1/2)*sqrt(15/pi))^2 * pi/4
    const float16_t c3 = float16_t(0.078125f); // ((1/4)*sqrt(5/pi))^2 * pi/4
    const float16_t c4 = float16_t(0.234375f); // ((1/4)*sqrt(15/pi))^2 * pi/4

    SH9 result;
    result.LM[0] = c0;
    
    result.LM[1] = c1 * dir.y;
    result.LM[2] = c1 * dir.z;
    result.LM[3] = c1 * dir.x;

    const f16vec3 dir2 = dir * dir;
    result.LM[4] = c2 * dir.x * dir.y;
    result.LM[5] = c2 * dir.y * dir.z;
    result.LM[6] = c3 * float16_t(3.0f * dir2.z - 1.0f);
    result.LM[7] = c2 * float16_t(dir.x * dir.z);
    result.LM[8] = c4 * float16_t(dir2.x - dir2.y);
    
    return result;
}

SH9Irradiance SH_irradiance_map(SH9RGB sh) {
    const float PI = 3.14159265359f;
    const float PI_INV = 1.0f /  PI;
    
    SH9Irradiance result;
    result.AR.x = PI_INV * float(sh.R.LM[3]);
    result.AR.y = PI_INV * float(sh.R.LM[1]);
    result.AR.z = PI_INV * float(sh.R.LM[2]);
    result.AR.w = PI_INV * float(sh.R.LM[0] - sh.R.LM[6]);
    result.AG.x = PI_INV * float(sh.G.LM[3]);
    result.AG.y = PI_INV * float(sh.G.LM[1]);
    result.AG.z = PI_INV * float(sh.G.LM[2]);
    result.AG.w = PI_INV * float(sh.G.LM[0] - sh.G.LM[6]);
    result.AB.x = PI_INV * float(sh.B.LM[3]);
    result.AB.y = PI_INV * float(sh.B.LM[1]);
    result.AB.z = PI_INV * float(sh.B.LM[2]);
    result.AB.w = PI_INV * float(sh.B.LM[0] - sh.B.LM[6]);
    
    
    result.BR.x = PI_INV * float(sh.R.LM[4]);
    result.BR.y = PI_INV * float(sh.R.LM[5]);
    result.BR.z = PI_INV * float(sh.R.LM[6]) * 3.0f;
    result.BR.w = PI_INV * float(sh.R.LM[7]);
    result.BG.x = PI_INV * float(sh.G.LM[4]);
    result.BG.y = PI_INV * float(sh.G.LM[5]);
    result.BG.z = PI_INV * float(sh.G.LM[6]) * 3.0f;
    result.BG.w = PI_INV * float(sh.G.LM[7]);
    result.BB.x = PI_INV * float(sh.B.LM[4]);
    result.BB.y = PI_INV * float(sh.B.LM[5]);
    result.BB.z = PI_INV * float(sh.B.LM[6]) * 3.0f;
    result.BB.w = PI_INV * float(sh.B.LM[7]);

    result.C.x = PI_INV * float(sh.R.LM[8]);
    result.C.y = PI_INV * float(sh.G.LM[8]);
    result.C.z = PI_INV * float(sh.B.LM[8]);
    result.C.w = 1.0f;
    
    return result;
}

SH9 SH_multiphy(SH9 sh, float16_t val) {
    SH9 result;
    [[unroll]]
    for (uint i = 0; i < 5 + 3 + 1; i++)
        result.LM[i] = sh.LM[i] * val;
    
    return result;
}

SH9RGB SH_multiply(SH9 sh, f16vec3 color) {
    SH9RGB result;
    
    result.R = SH_multiphy(sh, color.r);
    result.G = SH_multiphy(sh, color.g);
    result.B = SH_multiphy(sh, color.b);
    
    return result;
}

SH9 SH_add(SH9 a, SH9 b) {
    SH9 result;
    [[unroll]]
    for (uint i = 0; i < 5 + 3 + 1; i++)
        result.LM[i] = a.LM[i] + b.LM[i];
    
    return result;
}

SH9RGB SH_add(SH9RGB a, SH9RGB b) {
    SH9RGB result;
    result.R = SH_add(a.R, b.R);
    result.G = SH_add(a.G, b.G);
    result.B = SH_add(a.B, b.B);

    return result;
}