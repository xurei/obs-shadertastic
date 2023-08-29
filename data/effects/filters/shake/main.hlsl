// Common parameters for all shaders, as reference. Do not uncomment this (but you can remove it safely).
/*
uniform float time;            // Time since the shader is running. Goes from 0 to 1 for transition effects; goes from 0 to infinity for filter effects
uniform texture2d image;       // Texture of the source (filters only)
uniform texture2d tex_interm;  // Intermediate texture where the previous step will be rendered (for multistep effects)
uniform float upixel;          // Width of a pixel in the UV space
uniform float vpixel;          // Height of a pixel in the UV space
uniform float rand_seed;       // Seed for random functions
uniform int current_step;      // index of current step (for multistep effects)
*/

// Specific parameters of the shader. They must be defined in the meta.json file next to this one.
//uniform texture2d source;
uniform float audio_level;
uniform float audio_threshold;
uniform float zoom_level;
uniform float shake_level;
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

float4 EffectLinear(float2 uv)
{
    float shaking_level = 100.0 + audio_level - (100.0 + audio_threshold);
    if (shaking_level < 0.0) {
        shaking_level = 0.0;
    }
    shaking_level /= -1.0 * audio_threshold;

    shaking_level = pow(shaking_level, 1.5);

    if (shaking_level < audio_threshold) {
        shaking_level = 0.0;
    }

    float zoom_level_ratio = shaking_level * zoom_level;

    //Slightly zoom the uv
    uv = uv * (1.0-zoom_level_ratio) + (zoom_level_ratio * 0.5);

    float sin_u = max(-1.0, min(1.0, 2.0*sin(time * 150.323)));
    float sin_v = max(-1.0, min(1.0, 2.0*sin(time * 137.717)));

    uv[0] += sin_u * (zoom_level_ratio*shake_level) * 0.5;
    uv[1] += sin_v * (zoom_level_ratio*shake_level) * 0.5;

    return image.Sample(textureSampler, uv);
}
//----------------------------------------------------------------------------------------------------------------------

// You probably don't want to change anything from this point.

float4 PSEffect(FragData f_in) : TARGET
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
