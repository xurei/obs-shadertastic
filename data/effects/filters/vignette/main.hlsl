uniform float strength;
uniform float curvature;
uniform float inner;
uniform float outer;
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

float4 EffectLinear(float2 uv)
{
    float4 px = image.Sample(textureSampler, uv);
    //Calculate edge curvature
    float2 curve = pow(abs(uv*2.0-1.0),vec2(1.0/curvature));
    //Compute distance to edge
    float edge = pow(length(curve), curvature);
    //Compute vignette gradient and intensity
    float vignette = 1.0 - strength*smoothstep(inner, outer, edge);
    px.rgb *= vignette;
    return px;
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
