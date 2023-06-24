uniform int blur_level_x;
uniform int blur_level_y;
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

float gaussian(float x) {
    return exp(-x*x);
    //Approximation of the gaussian using cos
    //return (1.0 + cos(x*3.0));
}

float4 getGaussianU(float2 uv, int nb_samples) {
    float gaussian_sum = gaussian(0);
    float4 px_out = image.Sample(textureSampler, uv) * gaussian_sum;
    float nb_samples_f = float(nb_samples);
    for (int i=1; i<nb_samples; ++i) {
        float du = i*upixel;
        float4 px_right = image.Sample(textureSampler, float2(uv[0]+du, uv[1]));
        float4 px_left = image.Sample(textureSampler, float2(uv[0]-du, uv[1]));

        float k = gaussian(float(i) / nb_samples_f);
        px_out += (px_right + px_left)*k;
        gaussian_sum += 2*k;
    }
    return px_out / gaussian_sum;
}

float4 getGaussianV(float2 uv, int nb_samples) {
    float gaussian_sum = gaussian(0);
    float4 px_out = tex_interm.Sample(textureSampler, uv) * gaussian_sum;
    float nb_samples_f = float(nb_samples);
    for (int i=1; i<nb_samples; ++i) {
        float dv = i*vpixel;
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0], uv[1]+dv));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0], uv[1]-dv));

        float k = gaussian(float(i) / nb_samples_f);
        px_out += (px_right + px_left)*k;
        gaussian_sum += 2*k;
    }
    return px_out / gaussian_sum;
}

float4 EffectLinear(float2 uv)
{
    float u = uv[0];
    float v = uv[1];

    if (current_step == 0) {
        int nb_samples = max(1, blur_level_x);
        return getGaussianU(uv, nb_samples);
    }
    else {
        int nb_samples = max(1, blur_level_y);
        return getGaussianV(uv, nb_samples);
    }
}
//----------------------------------------------------------------------------------------------------------------------

float4 PSEffect(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in.uv);
    //rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);
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
