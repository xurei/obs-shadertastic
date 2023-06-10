// Common configuration for all shaders. You can remove this comment safely
/*
uniform float4x4 ViewProj;
uniform texture2d image;
uniform texture2d tex_a;
uniform texture2d tex_b;
uniform texture2d tex_interm;
uniform float transition_time;
uniform float upixel;
uniform float vpixel;
uniform float rand_seed;
uniform int current_step;
*/

// Custom arguments, referenced in the meta.json file of the effect
uniform float random_amount;

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

float fract(float v) {
	return v - floor(v);
}

float rand2(float2 co){
	float v = sin(dot(co, float2(12.9898, 78.233))) * 43758.5453;
	return fract(v);
}
float rand(float a, float b) {
	return rand2(float2(a, b));
}

float srgb_nonlinear_to_linear_channel(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_nonlinear_to_linear(float3 v)
{
	return float3(srgb_nonlinear_to_linear_channel(v.r), srgb_nonlinear_to_linear_channel(v.g), srgb_nonlinear_to_linear_channel(v.b));
}

float4 EffectLinear(FragData f_in)
{
	// -----> YOUR CODE GOES HERE <-----

	return tex_b.Sample(textureSampler, f_in.uv);
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
