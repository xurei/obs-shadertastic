uniform float breaking_point;
uniform float max_blur_level;
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

//    if (n < 0.5) {
//        if (n < 0.0) {
//            return 0.0;
//        }
//        return 4.0*n*n*n;
//    }
//    else {
//        if (n > 1.0) {
//            return 1.0;
//        }
//        float xx = -2.0*n+2.0;
//        return 1.0 - xx*xx*xx / 2.0;
//    }
}

float sigmoid_centered(float strength, float center, float n) {
    if (center < 0.5) {
        float width = 2 * center;
        return sigmoid(strength, n/width);
    }
    else {
        float width = 2 * (1-center);
        float m = (n - center + width/2) / width;
        return sigmoid(strength, m);
    }
}

float gaussian(float x) {
    return exp(-x*x) * 0.3;
}

float4 getGaussianU(float2 uv, int nb_samples, float ratio) {
    float4 px_out = tex_interm.Sample(textureSampler, uv);
    float gaussian_sum = 1.0;
    for (int i=1; i<nb_samples; ++i) {
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0]+i*upixel, uv[1]));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0]-i*upixel, uv[1]));

        float k = gaussian(ratio*i*upixel);
        px_out[0] += (px_right[0] + px_left[0])*k;
        px_out[1] += (px_right[1] + px_left[1])*k;
        px_out[2] += (px_right[2] + px_left[2])*k;
        px_out[3] += (px_right[3] + px_left[3])*k;
        gaussian_sum += 2*k;
    }
    return px_out / gaussian_sum;
}

float4 getGaussianV(float2 uv, int nb_samples, float ratio) {
    float4 px_out = tex_interm.Sample(textureSampler, uv);
    float gaussian_sum = 1.0;
    for (int i=1; i<nb_samples; ++i) {
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0], uv[1]+i*vpixel));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0], uv[1]-i*vpixel));
        float k = gaussian(ratio*i*vpixel);
        px_out[0] += px_right[0]*k + px_left[0]*k;
        px_out[1] += px_right[1]*k + px_left[1]*k;
        px_out[2] += px_right[2]*k + px_left[2]*k;
        px_out[3] += px_right[3]*k + px_left[3]*k;
        gaussian_sum += 2*k;
    }
    return px_out / gaussian_sum;
}

float4 EffectLinear(float2 uv)
{
    float u = uv[0];
    float v = uv[1];

    float break_point = breaking_point / 100.0;

    if (current_step == 0) {
        float lerp_ratio = sigmoid_centered(10, break_point, time);
        return lerp(
            tex_a.Sample(textureSampler, uv),
            tex_b.Sample(textureSampler, uv),
            lerp_ratio
        );
    }
    else {
        float t = time;
        if (t < break_point) {
            t = sigmoid_centered(5, break_point/2.0, t);
        }
        else {
            t = -1 * sigmoid_centered(5, break_point + (1.0-break_point) / 2.0, t) + 1;
        }

        float ln_0_01 = log(max_blur_level / 30.0); //4.605170186;
        int nb_samples = int(max(1, 10*max_blur_level * t));

        if (current_step == 1) {
            float blur_width = float(nb_samples) * upixel;
            float ratio = ln_0_01 / blur_width;
            return getGaussianU(uv, nb_samples, ratio);
        }
        else {
            float blur_width = float(nb_samples) * vpixel;
            float ratio = ln_0_01 / blur_width;
            return getGaussianV(uv, nb_samples, ratio);
        }
    }
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
