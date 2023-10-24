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
*/

// Specific parameters of the shader. They must be defined in the meta.json file next to this one.
uniform float displacement_strength_x;
uniform float displacement_strength_y;
uniform int color_space;
uniform int sampler_mode;
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

float3 rgbToYuv(float3 rgb)
{
    return float3(
        ( 0.257 * rgb.r + 0.504 * rgb.g + 0.098 * rgb.b) / 0.859,
        (-0.148 * rgb.r - 0.291 * rgb.g + 0.439 * rgb.b) / 0.439,
        ( 0.439 * rgb.r - 0.368 * rgb.g - 0.071 * rgb.b) / 0.439
    );
}

float applyBorders_single(float val) {
    if (val > 1.0) {
        if (sampler_mode == 0) { // Clamp
            return 1.0;
        }
        else if (sampler_mode == 1) { // Mirror
            return 1.0 - (val - 1.0);
        }
        else { // Repeat
            return val - floor(val);
        }
    }
    else if (val < 0.0) {
        if (sampler_mode == 0) { // Clamp
            return 0.0;
        }
        else if (sampler_mode == 1) { // Mirror
            return -1.0 * val;
        }
        else { // Repeat
            return val - floor(val);
        }
    }
    else {
        return val;
    }
}
float2 applyBorders(float2 uv) {
    return float2(
        applyBorders_single(uv[0]),
        applyBorders_single(uv[1])
    );
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear(float2 uv)
{
    float4 mask = tex_a.Sample(textureSampler, uv);
    float2 displacement = float2(0.0, 0.0);

    float lerp_breakpoint = 0.2;

    float t = smoothstep(lerp_breakpoint, 1.0, 1.0-time);

    if (color_space == 0) { // RGB
        displacement = t*float2(displacement_strength_x, displacement_strength_y) * ((mask.xy - 0.5) / 0.5);
    }
    else if (color_space == 1) { // YUV
        float3 mask_yuv = rgbToYuv(mask.rgb);
        displacement = t*float2(displacement_strength_x, displacement_strength_y) * float2(mask_yuv.y, mask_yuv.z);
    }
    float2 uv2 = uv - displacement;
    uv2 = applyBorders(uv2);
    float4 px = tex_b.Sample(textureSampler, uv2);

    if (time <= lerp_breakpoint) {
        return lerp(mask, px, smoothstep(0.05, 0.95, time / lerp_breakpoint));
    }

    return px;
}
//----------------------------------------------------------------------------------------------------------------------

// You probably don't want to change anything from this point.

float4 PSEffect(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in.uv);
    rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);
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
