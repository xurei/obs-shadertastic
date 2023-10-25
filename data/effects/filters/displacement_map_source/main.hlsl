uniform texture2d displacement_map;
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
//----------------------------------------------------------------------------------------------------------------------

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

float4 EffectLinear(float2 uv)
{
    float2 displacement_strength = float2(displacement_strength_x, displacement_strength_y);
    if (color_space == 0) { // RGB
        float4 mask = displacement_map.Sample(textureSampler, uv);
        float2 uv2 = uv - displacement_strength*float2((mask.x - 0.5)/0.5, (mask.y - 0.5)/0.5);
        uv2 = applyBorders(uv2);
        float4 px = image.Sample(textureSampler, uv2);
        return px;
    }
    else if (color_space == 1) { // YUV
        float4 mask = displacement_map.Sample(textureSampler, uv);
        float3 mask_yuv = rgbToYuv(mask.rgb);
        float2 uv2 = uv - displacement_strength*float2(mask_yuv.y, mask_yuv.z);
        uv2 = applyBorders(uv2);
        float4 px = image.Sample(textureSampler, uv2);
        return px;
    }
    else {
        // Should not happen
        return image.Sample(textureSampler, uv);
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
