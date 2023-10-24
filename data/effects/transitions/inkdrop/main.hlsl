// Common parameters for all shaders, as reference. Do not uncomment this (but you can remove it safely).
/*
uniform float time;            // Time since the shader is running. Goes from 0 to 1 for transition effects; goes from 0 to infinity for filter effects
uniform texture2d tex_a;       // Texture of the previous frame (transitions only)
uniform texture2d tex_b;       // Texture of the next frame (transitions only)
uniform texture2d tex_interm;  // Intermediate texture where the previous step will be rendered (for multistep effects)
uniform float upixel;          // Width of a pixel in the UV space
uniform float vpixel;          // Height of a pixel in the UV space
uniform float rand_seed;       // Seed for random functions
uniform int current_step;      // index of current step (for multistep effects)
uniform int nb_steps;          // number of steps (for multisteps effects)
*/

// Specific parameters of the shader. They must be defined in the meta.json file next to this one.
uniform float random_amount;
//----------------------------------------------------------------------------------------------------------------------

// These are required objects for the shader to work.
// You don't need to change anything here, unless you know what you are doing
sampler_state textureSampler {
    Filter    = Linear;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

struct VertData {
    float2 uv  : TEXCOORD0;
    float4 pos : POSITION;
};

struct FragData {
    float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
    VertData vert_out;
    vert_out.uv  = v_in.uv;
    vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
    return vert_out;
}
//----------------------------------------------------------------------------------------------------------------------

#define TAU 6.28318530718

#define SPEED 1.1

float rand(float2 n) {
    return fract(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

float noise(float2 p){
    float2 ip = floor(p);
    float2 u = float2(fract(p[0]), fract(p[1]));
    u = u*u*(3.0-2.0*u);

    float res = mix(
        mix(rand(ip+rand_seed),rand(ip+rand_seed+float2(1.0,0.0)),u.x),
        mix(rand(ip+rand_seed+float2(0.0,1.0)),rand(ip+rand_seed+float2(1.0,1.0)),u.x),u.y);
    return res*res;
}

float fbm(float2 p, int octaves)
{
    float n = 0.0;
    float a = 1.0;
    float norm = 0.0;
    for(int i = 0; i < octaves; ++i)
    {
        n += noise(p) * a;
        norm += a;
        p *= 2.0;
        a *= 0.5;
    }
    return n / norm;
}

float4 EffectLinear( float2 uv_orig )
{
    float2 uv = uv_orig - float2(0.5, 0.5);

    float angle_orig = atan2(uv.y, uv.x);
    float angle = angle_orig + fbm(uv * 4.0, 3) * 0.5;
    float2 p = float2(cos(angle), sin(angle));

    float t = time * SPEED;
    t *= t;

    float l = dot(uv / t, uv / t);
    l -= (fbm(normalize(uv) * 3.0, 5) - 0.5);

    float ink1 = fbm(p * 8.0, 4) + 1.5 - l;
    ink1 = clamp(ink1, 0.0, 1.0);

    float ink2 = fbm(p * (8.0+rand_seed), 4) + 1.5 - l;
    ink2 = 1.0 - clamp(ink2, 0.0, 1.0);

    float2 displace = (1.0 - time) * ink2 * uv; //float2(cos(angle), sin(angle)); //(ink2 * p_orig);

    float4 px_a = tex_a.Sample(textureSampler, uv_orig);
    float4 px_b = tex_b.Sample(textureSampler, uv_orig + (3.0 * displace));

    return lerp(px_a, px_b, ink1);
}
//----------------------------------------------------------------------------------------------------------------------

// You probably don't want to change anything from this point.

float4 PSEffect(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in.uv);
    if (current_step == nb_steps - 1) {
        rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);
    }
    return rgba;
}

float4 PSEffectLinear(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in.uv);
    return rgba;
}

technique Draw
{
    pass
    {
        vertex_shader = VSDefault(v_in);
        pixel_shader = PSEffect(f_in);
    }
}

technique DrawLinear
{
    pass
    {
        vertex_shader = VSDefault(v_in);
        pixel_shader = PSEffectLinear(f_in);
    }
}
