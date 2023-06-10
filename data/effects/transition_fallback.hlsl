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

float srgb_nonlinear_to_linear_channel(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_nonlinear_to_linear(float3 v)
{
	return float3(srgb_nonlinear_to_linear_channel(v.r), srgb_nonlinear_to_linear_channel(v.g), srgb_nonlinear_to_linear_channel(v.b));
}

float4 PSEffectLinear(FragData f_in) : TARGET
{
	float4 b_val = tex_b.Sample(textureSampler, f_in.uv);
	b_val.g = 0.0;
	b_val.b = 0.0;
	return b_val;
}

float4 PSEffect(FragData f_in) : TARGET
{
    float4 rgba = PSEffectLinear(f_in);
	rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);
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
