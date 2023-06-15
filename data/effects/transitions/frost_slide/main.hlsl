// Custom arguments, referenced in the meta.json file of the effect
uniform float random_amount;
uniform float angle;
uniform float frost_width;
uniform int direction;
uniform float max_blur_level;
uniform float white_mask;
uniform float frost_strength;

#define sampleTex(is_a, uv) ((is_a) ? tex_a.Sample(textureSampler, uv) : tex_b.Sample(textureSampler, uv))
#define PI 3.1415926535

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

float2 rotate(float2 uv, float angleDeg) {
    float angleRad = angleDeg * (2*PI/360.0);
    return float2(
        cos(angleRad) * uv[0] - sin(angleRad) * uv[1],
        sin(angleRad) * uv[0] + cos(angleRad) * uv[1]
    );
}

float rand2(float2 co){
    float v = sin(dot(co, float2(12.9898, 78.233))) * 43758.5453;
    return fract(v);
}
float rand(float a, float b) {
    return rand2(float2(a, b));
}

float sigmoid(float strength, float n) {
    // Vanilla sigmoid ; this is not symetric between 0 and 1
    //return (1 / (1 + exp(-n)));

    n = strength * (n*2 - 1);
    float v0 = (1 / (1 + exp(strength)));
    float v1 = 1 - v0; // simplified from v1 = (1 / (1 + exp(-1 * strength)));

    return (1 / (1 + exp(-n)) - v0) / (v1-v0);
}

float sinplus1(float v) {
    return (1.0 + sin(v)) / 2.0;
}

float cosplus1(float v) {
    return (1.0 + cos(v)) / 2.0;
}

float sinwaves(float val) {
    return (
        0.2*sinplus1(17.0*val) +
        0.1*cosplus1(23.0*val) +
        0.2*sinplus1(47.0*val) +
        0.15*sinplus1(53.0*val) +
        0.15*cosplus1(97.0*val) +
        0.11*sinplus1(197.0*val)
    ) / (0.2+0.1+0.2+0.15+0.15+0.11);
}

float gaussian(float x) {
    return exp(-x*x);
}

float4 getGaussianU(float2 uv, int nb_samples) {
    float gaussian_sum = gaussian(0);
    float4 px_out = tex_interm.Sample(textureSampler, uv) * gaussian_sum;
    float nb_samples_f = float(nb_samples);
    #ifdef _D3D11
    [loop]
    #endif
    for (int i=1; i<nb_samples; ++i) {
        float du = i*upixel;
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0]+du, uv[1]));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0]-du, uv[1]));

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
    #ifdef _D3D11
    [loop]
    #endif
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

bool isFrost(float2 uv, float2 bar_orig, float2 bar_vec) {
    if (direction == 2) { //left
        return (bar_orig[0] < uv[0] && uv[0] < bar_orig[0]+frost_width/100.0);
    }
    else { //right
        return (bar_orig[0] > uv[0] && uv[0] > bar_orig[0]-frost_width/100.0);
    }
}
bool isSceneA(float2 uv, float2 bar_orig, float2 bar_vec) {
    if (direction == 2) { //left
        return (bar_orig[0] > uv[0]);
    }
    else { //right
        return (bar_orig[0] < uv[0]);
    }
}
bool isBorderA(float2 uv, float2 bar_orig, float2 bar_vec) {
    if (direction == 2) { //left
        return (
            (bar_orig[0] > uv[0] - 4.0*upixel)
        );
    }
    else { //right
        return (
            (bar_orig[0] < uv[0] + 4.0*upixel)
        );
    }
}
bool isBorderB(float2 uv, float2 bar_orig, float2 bar_vec) {
    if (direction == 2) { //left
        float border_start = bar_orig[0]+frost_width/100.0;
        return (border_start < uv[0] && uv[0] < border_start + 4.0*upixel);
    }
    else { //right
        float border_start = bar_orig[0]-frost_width/100.0;
        return (border_start > uv[0] && uv[0] > border_start - 4.0*upixel);
    }
}

float4 EffectLinear(FragData f_in)
{
    float t = sigmoid(1.5, time);
    float2 uv = f_in.uv;
    float u = f_in.uv[0];
    float v = f_in.uv[1];
    //float angle_rad = angle * 0.017453293;

    float2 bar_vec = rotate(float2(1.0, 0.0), angle);

//    if (uv[0] < 0.1 && uv[1] < 0.1) {
//        float4 result = float4(bar_vec[0], -bar_vec[0], 0.0, 1.0);
//        return result;
//    }
//    else if (uv[0] < 0.1 && uv[1] < 0.2) {
//        float4 result = float4(0.0, -bar_vec[1], bar_vec[1], 1.0);
//        return result;
//    }

    float bar_displace_for_v = bar_vec[0]/bar_vec[1];
    float bar_orig_initial;
    float bar_orig_final;
    if (direction == 2) { //left
        bar_orig_initial = 1.0 + (angle < 90 ? 0.0 : abs(bar_displace_for_v));
        bar_orig_final = -frost_width/100.0 - (angle < 90 ? abs(bar_displace_for_v) : 0.0);
    }
    else { //right
        bar_orig_initial = 0.0 - (angle < 90 ? abs(bar_displace_for_v) : 0.0);
        bar_orig_final = 1.0 + frost_width/100.0 + (angle < 90 ? 0.0 : abs(bar_displace_for_v));
    }
    float2 bar_orig = float2(lerp(bar_orig_initial, bar_orig_final, t), 0.0);
    bar_orig = float2(bar_orig[0] + bar_displace_for_v * uv[1], bar_orig[1]);

    if (isSceneA(uv, bar_orig, bar_vec)) {
        return tex_a.Sample(textureSampler, uv);
    }
    else if (isBorderA(uv, bar_orig, bar_vec)) {
        float4 result = tex_a.Sample(textureSampler, uv);
        result += float4(0.5, 0.5, 0.5, 0.0);

        return result;
    }
    else if (isFrost(uv, bar_orig, bar_vec)) {
//        float4 result = float4(0.0, 0.7, 0.0, 1.0);
//        return result;

        int nb_samples = int(max(1, max_blur_level*10));

        float u2 = round(u*0.1/upixel)/(0.1/upixel) + sinwaves(u*0.0001);
        float v2 = round(v*0.1/vpixel)/(0.1/vpixel) + sinwaves(v*0.0001);

        float2 displace = float2(
            (-0.5+rand(rand_seed + u2, rand_seed - v2))*frost_strength*upixel,
            (-0.5+rand(rand_seed - u2, rand_seed + v2))*frost_strength*vpixel
        );

        if (current_step == 0) {
            float4 result = tex_a.Sample(textureSampler, uv + displace);
//            result[0] = min(1.0, result[0] + white_mask);
//            result[1] = min(1.0, result[1] + white_mask);
//            result[2] = min(1.0, result[2] + white_mask);
            return result; //lerp(result, px_frost, frost_strength/100.0);
        }
        else if (current_step == 1) {
            float4 result = getGaussianV(uv, nb_samples);
            return result;
            //return DrawCurve(u, v, gaussian(ratio*(u-0.5), nb_samples));
        }
        else {
            //return DrawCurve(u, v, gaussian(ratio*(u-0.5), nb_samples));
            float2 uv2 = uv + displace;
            uv2[0] = min(uv2[0], bar_orig[0] + frost_width/100.0 - frost_strength*upixel);
            float4 result = getGaussianU(uv2, nb_samples);
            result[0] = min(1.0, result[0] + white_mask);
            result[1] = min(1.0, result[1] + white_mask);
            result[2] = min(1.0, result[2] + white_mask);
            return result; //lerp(result, px_frost, frost_strength/100.0);
        }
    }
    else if (isBorderB(uv, bar_orig, bar_vec)) {
        float4 result = tex_b.Sample(textureSampler, uv);
        result += float4(0.5, 0.5, 0.5, 0.0);

        return result;
    }
    else {
        return tex_b.Sample(textureSampler, uv);
    }
}

float4 PSEffect(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in);
    rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);
    return rgba;
}

float4 PSEffectLinear(FragData f_in) : TARGET
{
    float4 rgba = EffectLinear(f_in);
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
