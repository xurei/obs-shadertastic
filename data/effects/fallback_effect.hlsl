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

bool isR(float ref_u, float2 uv) {
    //0.4, 0.1
    if (uv.x > ref_u && uv.x < ref_u+0.2 && uv.y > 0.1 && uv.y < 0.2) {
        return true;
    }
    if (uv.x > ref_u && uv.x < ref_u+0.2 && uv.y > 0.45 && uv.y < 0.55) {
        return true;
    }
    if (uv.x > ref_u && uv.x < ref_u+0.05 && uv.y > 0.1 && uv.y < 0.9) {
        return true;
    }
    if (uv.x > ref_u+0.15 && uv.x < ref_u+0.2 && uv.y > 0.1 && uv.y < 0.5) {
        return true;
    }
    if ((uv.x-uv.y*0.3+0.5/3.0) > ref_u+0.06 && (uv.x-uv.y*0.3+0.5/3.0) < ref_u+0.12 && uv.y > 0.55 && uv.y < 0.9) {
        return true;
    }
    return false;
}

float4 EffectLinear(float2 uv)
{
    float4 errmsg_color = float4(0.0, 0.0, 0.0, 0.0);

    // E
    if (uv.x > 0.1 && uv.x < 0.3 && uv.y > 0.1 && uv.y < 0.2) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }
    else if (uv.x > 0.1 && uv.x < 0.3 && uv.y > 0.8 && uv.y < 0.9) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }
    else if (uv.x > 0.1 && uv.x < 0.3 && uv.y > 0.45 && uv.y < 0.55) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }
    else if (uv.x > 0.1 && uv.x < 0.15 && uv.y > 0.1 && uv.y < 0.9) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }

    // R 1
    else if (isR(0.4, uv)) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }

    // R 2
    else if (isR(0.7, uv)) {
        errmsg_color = float4(1.0, 1.0, 1.0, 0.75);
    }

    float4 img_px = image.Sample(textureSampler, uv);
    float4 texb_px = tex_b.Sample(textureSampler, uv);
    float4 px = texb_px + img_px;
    px[3] = 1.0;
    px *= float4(1.0, 0.0, 0.0, 1.0);// * (tex_b.Sample(textureSampler, uv));

    return lerp(px, errmsg_color, errmsg_color.a);
}

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
