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
uniform int square_width_px;
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
    float square_width = square_width_px*upixel;
    float square_height = square_width * vpixel/upixel; //0.05;
    int square_u = int(uv[0]/square_width);
    int square_v = int(uv[1]/square_height);

    if (current_step == 0) {
        float4 pixel_sum = float4(0.0, 0.0, 0.0, 0.0);

        #ifdef _D3D11
        [loop]
        #endif
        for (int du=0; du<square_width_px; du++) {
            pixel_sum += image.Sample(textureSampler, float2(float(square_u*square_width + du*upixel), uv[1]));
        }
        return pixel_sum / square_width_px;
    }
    else {
        float4 pixel_sum = float4(0.0, 0.0, 0.0, 0.0);

        #ifdef _D3D11
        [loop]
        #endif
        for (int dv=0; dv<square_width_px; dv++) {
            pixel_sum += tex_interm.Sample(textureSampler, float2(uv[0], float(square_v*square_height + dv*vpixel)));
        }
        return pixel_sum / square_width_px;
    }
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
