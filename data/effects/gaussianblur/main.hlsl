// Custom arguments, referenced in the meta.json file of the effect
uniform float breaking_point;
uniform float max_blur_level;

#define sampleTex(is_a, uv) ((is_a) ? tex_a.Sample(textureSampler, uv) : tex_b.Sample(textureSampler, uv))

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

float sigmoid(float strength, float n) {
	// Vanilla sigmoid ; this is not symetric between 0 and 1
	//return (1 / (1 + exp(-n)));

	n = strength * (n*2 - 1);
	float v0 = (1 / (1 + exp(strength)));
	float v1 = (1 / (1 + exp(-1 * strength)));

	return (1 / (1 + exp(-n)) - v0) / (v1-v0);
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

float srgb_nonlinear_to_linear_channel(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_nonlinear_to_linear(float3 v)
{
	return float3(srgb_nonlinear_to_linear_channel(v.r), srgb_nonlinear_to_linear_channel(v.g), srgb_nonlinear_to_linear_channel(v.b));
}

#define SQRT_PI 1.772453851

float gaussian(float x, int nb_samples) {
    return exp(-x*x) * 0.3;
}

float4 getGaussianU(float2 uv, int nb_samples, float ratio) {
    float4 px_out = tex_interm.Sample(textureSampler, uv);
    float gaussian_sum = 1.0;
    for (int i=1; i<nb_samples; ++i) {
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0]+i*upixel, uv[1]));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0]-i*upixel, uv[1]));

        float k = gaussian(ratio*i*upixel, nb_samples);
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
        float k = gaussian(ratio*i*vpixel, nb_samples);
        px_out[0] += px_right[0]*k + px_left[0]*k;
        px_out[1] += px_right[1]*k + px_left[1]*k;
        px_out[2] += px_right[2]*k + px_left[2]*k;
        px_out[3] += px_right[3]*k + px_left[3]*k;
        gaussian_sum += 2*k;
    }
    return px_out / gaussian_sum;
}

#include "../devtools.hlsl"
float4 EffectLinear(FragData f_in)
{
    float u = f_in.uv[0];
    float v = f_in.uv[1];

    float break_point = breaking_point / 100.0;
	float t = transition_time;
	if (t < break_point) {
	    t = sigmoid_centered(5, break_point/2.0, t);
	}
	else {
	    t = -1 * sigmoid_centered(5, break_point + (1.0-break_point) / 2.0, t) + 1;
	}
	//return DrawCurve(u, v, t);

    float ln_0_01 = log(max_blur_level / 30.0); //4.605170186;

    int nb_samples = int(max(1, 100 * t));

    float width = 1.0/upixel;
    float height = 1.0/vpixel;
    float blur_width;
    float ratio;

    //float lerp_ratio = sigmoid_centered(8, break_point, u);
    //return DrawCurve(u, v, lerp_ratio);
    if (current_step == 0) {
        float lerp_ratio = sigmoid_centered(8, break_point, transition_time);
        return lerp(
            tex_a.Sample(textureSampler, f_in.uv),
            tex_b.Sample(textureSampler, f_in.uv),
            lerp_ratio
        );
    }
    else if (current_step == 1) {
        blur_width = float(nb_samples) / width;
        ratio = ln_0_01 / blur_width;
        return getGaussianU(f_in.uv, nb_samples, ratio);
    }
    else {
        blur_width = float(nb_samples) / height;
        ratio = ln_0_01 / blur_width;
        return getGaussianV(f_in.uv, nb_samples, ratio);
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

technique Effect
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSEffect(f_in);
	}
}

technique EffectLinear
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSEffectLinear(f_in);
	}
}
