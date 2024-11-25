uniform float strength;
uniform float curvature;
uniform float inner;
uniform float outer;
uniform bool keep_aspect_ratio;
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
    if (keep_aspect_ratio) {
        if (vpixel > upixel) {
            uv.x -= 0.5;
            uv.x *= vpixel/upixel;
            uv.x += 0.5;
        }
        else {
            uv.y -= 0.5;
            uv.y *= upixel/vpixel;
            uv.y += 0.5;
        }
    }
    //Calculate edge curvature
    float inv_curvature = 1.0 / curvature;
    float2 curve = pow(abs(uv*2.0-1.0),float2(inv_curvature,inv_curvature));
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
