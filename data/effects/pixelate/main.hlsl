uniform float breaking_point;
uniform float max_pixelation_level;
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

float sigmoid(float strength, float n) {
    // Vanilla sigmoid ; this is not symetric between 0 and 1
    //return (1 / (1 + exp(-n)));

    n = strength * (n*2 - 1);
    float v0 = (1 / (1 + exp(strength)));
    float v1 = 1 - v0; // simplified from v1 = (1 / (1 + exp(-1 * strength)));

    return (1 / (1 + exp(-n)) - v0) / (v1-v0);
}

float4 EffectLinear(float2 uv)
{
    float u = uv[0];
    float v = uv[1];
    float break_point = breaking_point / 100.0;

    float size_ratio = (time < break_point) ? time / break_point : (1.0-time) / (1.0-break_point);

    float min_squares = 100.0 / max_pixelation_level;

    //float u_width = 20.0 + (1.0/upixel - 20.0) * (1.0 - pow(size_ratio, 0.08));
    float u_width = min_squares + (1.0/upixel - min_squares) * (1.0 - pow(size_ratio, 0.05));
    float v_width = u_width / vpixel * upixel;

    u = floor(u * u_width) / u_width + 0.5/u_width;
    v = floor(v * v_width) / v_width + 0.5/v_width;

    float4 px_a = tex_a.Sample(textureSampler, float2(u,v));
    float4 px_b = tex_b.Sample(textureSampler, float2(u,v));

    float lerp_t = sigmoid(30, (time+0.5-break_point));

    px_a.r = lerp(px_a.r, 1.0, 0.1*lerp_t);
    px_a.g = lerp(px_a.g, 1.0, 0.1*lerp_t);
    px_a.b = lerp(px_a.b, 1.0, 0.1*lerp_t);
    px_b.r = lerp(px_b.r, 1.0, 0.1*(1-lerp_t));
    px_b.g = lerp(px_b.g, 1.0, 0.1*(1-lerp_t));
    px_b.b = lerp(px_b.b, 1.0, 0.1*(1-lerp_t));

    return lerp(
        px_a,
        px_b,
        lerp_t
    );
}
//----------------------------------------------------------------------------------------------------------------------

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
