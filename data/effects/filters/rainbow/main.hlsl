uniform float random_amount;
//----------------------------------------------------------------------------------------------------------------------

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

float4 HueShift (float4 pixel, float Shift)
{
    float3 P = float3(0.55735,0.55735,0.55735)*dot(float3(0.55735,0.55735,0.55735),pixel.xyz);
    float3 U = pixel.xyz-P;
    float3 V = cross(float3(0.55735,0.55735,0.55735),U);

    pixel.xyz = U*cos(Shift*6.2832) + V*sin(Shift*6.2832) + P;
    return float4(pixel.xyz, pixel.a);
}

float4 EffectLinear(float2 uv)
{
    float4 pixel = image.Sample(textureSampler, uv);
    pixel = HueShift(pixel, time - int(time));
    return pixel;
}
//----------------------------------------------------------------------------------------------------------------------

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
