// Common parameters for all shaders, as reference. Do not uncomment this (but you can remove it safely).
/*
uniform float time;            // Time since the shader is running. Goes from 0 to 1 for transition effects; goes from 0 to infinity for filter effects
uniform texture2d image;       // Texture of the source (filters only)
uniform texture2d tex_interm;  // Intermediate texture where the previous step will be rendered (for multistep effects)
uniform float upixel;          // Width of a pixel in the UV space
uniform float vpixel;          // Height of a pixel in the UV space
uniform float rand_seed;       // Seed for random functions
uniform int current_step;      // index of current step (for multistep effects)
*/

// Specific parameters of the shader. They must be defined in the meta.json file next to this one.
uniform float2 fd_leye_1;
uniform float2 fd_leye_2;
uniform float2 fd_reye_1;
uniform float2 fd_reye_2;
uniform float2 fd_face_1;
uniform float2 fd_face_2;
uniform texture2d fd_points_tex;

uniform float audio_level;
uniform float audio_threshold_low;
uniform float audio_threshold_high;
uniform float eye_intensity_ratio;
uniform float intensity;
uniform float audio_impact;
//----------------------------------------------------------------------------------------------------------------------

#define PI 3.1415926535
//----------------------------------------------------------------------------------------------------------------------

// These are required objects for the shader to work.
// You don't need to change anything here, unless you know what you are doing
sampler_state textureSampler {
    Filter    = Linear;
    AddressU  = Clamp;
    AddressV  = Clamp;
};
sampler_state pointsSampler {
    Filter    = Point;
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

float3 rgb2yuv(float3 rgb) {
    return float3(
        ( 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b),
        (-0.148 * rgb.r - 0.291 * rgb.g + 0.439 * rgb.b),
        ( 0.439 * rgb.r - 0.368 * rgb.g - 0.071 * rgb.b)
    );
}
//----------------------------------------------------------------------------------------------------------------------

float rand2(float2 co){
	float v = sin(dot(co, float2(12.9898, 78.233))) * 43758.5453;
	return fract(v);
}
float rand(float a, float b) {
	return rand2(float2(a, b));
}
float cosplus1(float v) {
	return (1.0 + cos(v)) / 2.0;
}
float sinplus1(float v) {
	return (1.0 + sin(v)) / 2.0;
}
//----------------------------------------------------------------------------------------------------------------------

float triangleArea(float2 p1, float2 p2, float2 p3) {
    return 0.5 * ((p2.x - p1.x) * (p3.y - p1.y) - (p3.x - p1.x) * (p2.y - p1.y));
}
//----------------------------------------------------------------------------------------------------------------------

float2 barycentricCoordinates(float2 p1, float2 p2, float2 p3, float2 point_to_check) {
    float totalArea = triangleArea(p1, p2, p3);
    float u = triangleArea(p3, p1, point_to_check) / totalArea;
    float v = triangleArea(p1, p2, point_to_check) / totalArea;
    return float2(u, v);
}
//----------------------------------------------------------------------------------------------------------------------

int3 get_best_triangle(float2 uv) {
    int3 triangles[28]; {
        int k = 0;
        // LEFT EYE
        triangles[k++] = int3(  7, 163, 246);
        triangles[k++] = int3(  7, 246,  33);
        triangles[k++] = int3(144, 161, 163);
        triangles[k++] = int3(144, 160, 161);
        triangles[k++] = int3(145, 159, 160);
        triangles[k++] = int3(161, 246, 163);
        triangles[k++] = int3(144, 145, 160);
        triangles[k++] = int3(145, 153, 159);
        triangles[k++] = int3(153, 158, 159);
        triangles[k++] = int3(153, 154, 158);
        triangles[k++] = int3(154, 157, 158);
        triangles[k++] = int3(154, 155, 157);
        triangles[k++] = int3(155, 173, 157);
        triangles[k++] = int3(133, 173, 155);

        // RIGHT EYE
        triangles[k++] = int3(249, 263, 466);
        triangles[k++] = int3(249, 466, 390);
        triangles[k++] = int3(362, 382, 398);
        triangles[k++] = int3(373, 387, 374);
        triangles[k++] = int3(373, 388, 387);
        triangles[k++] = int3(373, 390, 388);
        triangles[k++] = int3(374, 386, 380);
        triangles[k++] = int3(374, 387, 386);
        triangles[k++] = int3(380, 385, 381);
        triangles[k++] = int3(380, 386, 385);
        triangles[k++] = int3(381, 384, 382);
        triangles[k++] = int3(381, 385, 384);
        triangles[k++] = int3(382, 384, 398);
        triangles[k++] = int3(388, 390, 466);
    }

    float2 best_uv = float2(-1.0, -1.0);
    float best_z = 10000.0;
    int3 best_triangle = int3(-1, -1, -1);
    int3 prev_triangle = int3(-1, -1, -1);
    float4 edge1;
    float4 edge2;
    float4 edge3;
    for (int i=0; i<28; ++i) {
        int3 current_triangle = triangles[i];
        if (current_triangle[0] != prev_triangle[0]) {
            edge1 = fd_points_tex.Sample(pointsSampler, float2((current_triangle[0] + 0.5)/468.0, 0.0));
        }
        if (current_triangle[1] == prev_triangle[2]) {
            edge2 = edge3;
            edge3 = fd_points_tex.Sample(pointsSampler, float2((current_triangle[2] + 0.5)/468.0, 0.0));
        }
        else if (current_triangle[2] == prev_triangle[1]) {
            edge3 = edge2;
            edge2 = fd_points_tex.Sample(pointsSampler, float2((current_triangle[1] + 0.5)/468.0, 0.0));
        }
        else {
            edge2 = fd_points_tex.Sample(pointsSampler, float2((current_triangle[1] + 0.5)/468.0, 0.0));
            edge3 = fd_points_tex.Sample(pointsSampler, float2((current_triangle[2] + 0.5)/468.0, 0.0));
        }
        prev_triangle = current_triangle;

        if (uv.x < edge1.x && uv.x < edge2.x && uv.x < edge3.x) {
            continue;
        }
        if (uv.x > edge1.x && uv.x > edge2.x && uv.x > edge3.x) {
            continue;
        }
        if (uv.y < edge1.y && uv.y < edge2.y && uv.y < edge3.y) {
            continue;
        }
        if (uv.y > edge1.y && uv.y > edge2.y && uv.y > edge3.y) {
            continue;
        }

        float2 triangleUV = barycentricCoordinates(edge1.xy, edge2.xy, edge3.xy, uv);
        if ((0 <= triangleUV.x && triangleUV.x <= 1) && (0 <= triangleUV.y && triangleUV.y <= 1) && (triangleUV.x + triangleUV.y <= 1)) {
            if (edge1.z < best_z) {
                best_triangle = current_triangle;
                best_uv = triangleUV;
                best_z = edge1.z;
            }
        }
    }

    return best_triangle;
}
//----------------------------------------------------------------------------------------------------------------------

float gaussian(float x) {
    return exp(-x*x);
}

float4 getGaussianU(float2 uv, int nb_samples) {
    float gaussian_sum = gaussian(0);
    float4 px_out = tex_interm.Sample(textureSampler, uv) * gaussian_sum;
    float nb_samples_f = float(nb_samples);
    for (int i=1; i<nb_samples; ++i) {
        float du = i*upixel;
        float4 px_right = tex_interm.Sample(textureSampler, float2(uv[0]+du, uv[1]));
        float4 px_left = tex_interm.Sample(textureSampler, float2(uv[0]-du, uv[1]));

        float k = gaussian(float(i) / nb_samples_f);
        px_right.rgb *= px_right.a;
        px_left.rgb *= px_left.a;
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
    px_out /= gaussian_sum;
    return px_out;
}
//----------------------------------------------------------------------------------------------------------------------

float audio_effect_level() {
    float out_level = clamp(audio_level, -100.0, 0.0);
    out_level = 100.0 / (audio_threshold_high - audio_threshold_low) * (out_level - audio_threshold_low);
    out_level /= 100.0;
    out_level = clamp(out_level, 0.0, 1.0);
    return pow((1.0-audio_impact) + out_level * audio_impact, 1.5);
}
//----------------------------------------------------------------------------------------------------------------------

float eyesColoration(float2 uv, float2 fd_eye_1, float2 fd_eye_2, float eyeIntensity, float eye_seed) {
    float aspectRatio = vpixel/upixel;
    float2 orthoCorrection = float2(aspectRatio, 1.0);

    float2 position = (fd_eye_1 + fd_eye_2) * orthoCorrection / 2.0;
    float2 uvOrthonormal = uv * orthoCorrection;
    float2 uvLocal = uvOrthonormal - position;

    float3 best_triangle = get_best_triangle(uv);

    float minValue = 0.0;

    float audio_effect_level2 = audio_effect_level();
    float audio_level_mult = audio_effect_level2;

    if (best_triangle[0] > -1) {
        float2 fd_eye_center = ((fd_eye_1 + fd_eye_2) / 2.0) * (orthoCorrection);
        float fd_eye_radius = abs(fd_eye_1.y - fd_eye_center.y) * 1.20;
        float iris_dist = distance(uvOrthonormal, fd_eye_center);
        float pupil_radius = fd_eye_radius * 0.5;

        float angle = atan2(uvLocal.y, uvLocal.x);
        minValue = 0.3 + (smoothstep(0.0, 0.5, 1.0-clamp(iris_dist / fd_eye_radius, 0.0, 1.0)))*0.8*(
              6.7*smoothstep(0.2, 1.0, (1.0 - iris_dist / fd_eye_radius))
            + 2.5*sinplus1(iris_dist / fd_eye_radius)*pow(1.2*cosplus1(PI/2.0 + uvLocal.x*4.0)*cosplus1(uvLocal.y*4.0)*rand2(uvLocal) + cosplus1(eye_seed+(7.0-0.1*rand2(uv)+eye_seed*0.2)*angle) + 0.45*sinplus1((eye_seed+8.2)*(angle+0.5*rand2(uv))), 0.7)
        );
        if (iris_dist < pupil_radius * audio_level_mult) {
            minValue = max(minValue, 0.2 + 10.0 * clamp(1.0 - pow(iris_dist / pupil_radius, 0.8), 0.0, 1.0));
            //minValue = max(minValue, 1.5 + 5.0 * smoothstep(0.0, 10.0, (1.0-clamp(iris_dist / fd_eye_radius, 0.0, 1.0))));
            //minValue = 0.0;
        }
        else if (iris_dist < fd_eye_radius) {
            // Nothing to do
        }
        else {
            minValue = 0.2;
        }
    }

    return eyeIntensity * minValue;
}
//----------------------------------------------------------------------------------------------------------------------

float laserIntensity(float2 uv, float2 fd_eye_1, float2 fd_eye_2, float width, float eyes_dist, float compress_ratio_v) {
    float aspectRatio = vpixel/upixel;
    float2 orthoCorrection = float2(aspectRatio, 1.0) / eyes_dist;

    float2 uvOrthonormal = uv * orthoCorrection;
    float2 position = (fd_eye_1 + fd_eye_2) * orthoCorrection / 2.0;
    float2 eye_dimensions = abs(fd_eye_2-fd_eye_1);

    float2 uvLocal = uvOrthonormal - position;
    float uvLocalLength = length(uvLocal);

    float uvLocalLengthCompressed = length(uvLocal * float2(1.0, compress_ratio_v));
    uvLocalLengthCompressed = pow(uvLocalLengthCompressed, 1.9);

    float audio_effect_level2 = audio_effect_level();
    float audio_level_mult = audio_effect_level2;

    float intensityRay = 0.0;
    float intensityHalo = width * clamp(audio_level_mult * intensity/uvLocalLengthCompressed, 0.0, 10.0); //intensity*width*(pow(1.0-uvLocalLength, 10)) > 0.15 ? 1.0:0.2;

    return clamp(intensityRay + intensityHalo, 0.0, 5.0);
}
//----------------------------------------------------------------------------------------------------------------------

float3 perpendicularVector(float3 A, float3 B, float3 C) {
    // Calculate the vectors AB and AC
    float3 AB = B - A;
    float3 AC = C - A;

    // Calculate the cross product of AB and AC
    float3 normal = cross(AB, AC);

    return normalize(normal);
}
//----------------------------------------------------------------------------------------------------------------------

float laserRay(float2 uv, float2 lineStart, float2 direction) {
    // Project vecToCurrent onto the lineVector to find the closest point on the line
    float2 lineDirNormalized = normalize(direction.xy);
    float2 vecToCurrent = uv - lineStart;
    float projectionLength = dot(vecToCurrent, lineDirNormalized);
    float2 closestPoint = lineStart + lineDirNormalized * projectionLength;

    // Compute the distance from the current fragment to the closest point on the line
    float dist = length(uv - closestPoint);

    // If the distance is less than half the line width, color the fragment with the line color
    if (projectionLength < 0 && dist < 0.008 * 0.5) {
        return 1.0;
    }
    return 0.0;
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear__step0(float2 uv) {
    float2 positionL = (fd_leye_1 + fd_leye_2) / 2.0;
    float2 positionR = (fd_reye_1 + fd_reye_2) / 2.0;

    float eye_intensity = 2.0;
    eye_intensity *= eye_intensity_ratio;

    float intensityOfEyes = (
          eyesColoration(uv, fd_leye_1, fd_leye_2, eye_intensity, 1.344)
        + eyesColoration(uv, fd_reye_1, fd_reye_2, eye_intensity, 2.04)
    );

    float4 px = float4(0.0, 0.0, 0.0, 0.0);

    if (intensityOfEyes > 0.0001) {
        float4 laserColor = float4(0.0, 0.0, 0.0, 1.0); //intensityOfLaser * float4(0.8, 0.1, 0.1, 0.0);
        px = image.Sample(textureSampler, uv);
        laserColor.r = clamp(intensityOfEyes*1.00, 0.0, 1.0);
        laserColor.g = clamp(intensityOfEyes*0.12, 0.0, 0.85);
        laserColor.b = clamp(intensityOfEyes*0.10, 0.0, 0.8);
        px += laserColor;
        px.r = clamp(px.r, 0.0, 1.0);
        px.g = clamp(px.g, 0.0, 1.0);
        px.b = clamp(px.b, 0.0, 1.0);
    }

    return px;
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear__step1(float2 uv) {
    int nb_samples = 3;
    return getGaussianU(uv, nb_samples);
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear__step2(float2 uv) {
    int nb_samples = 3;
    return getGaussianV(uv, nb_samples);
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear__step3(float2 uv) {
    float4 leye_point = fd_points_tex.Sample(pointsSampler, float2((33 + 0.5)/468.0, 0.0));
    float4 reye_point = fd_points_tex.Sample(pointsSampler, float2((263 + 0.5)/468.0, 0.0));
    float4 chin_point = fd_points_tex.Sample(pointsSampler, float2((152 + 0.5)/468.0, 0.0));
    float3 direction = perpendicularVector(leye_point.xyz, reye_point.xyz, chin_point.xyz);

    float4 ppx = image.Sample(textureSampler, uv);

    if (fd_leye_1.x < 0.0) {
        return image.Sample(textureSampler, uv);
    }
    float4 px_interm = tex_interm.Sample(textureSampler, uv);

    float2 positionL = (fd_leye_1 + fd_leye_2) / 2.0;
    float2 positionR = (fd_reye_1 + fd_reye_2) / 2.0;

    float intensityOfLaser = (
          laserIntensity(uv, fd_leye_1, fd_leye_2, 0.7, distance(positionL, positionR), 50.0)
        + laserIntensity(uv, fd_reye_1, fd_reye_2, 0.7, distance(positionL, positionR), 50.0)
        + 0.5 * laserIntensity(uv, fd_leye_1, fd_leye_2, 0.6, distance(positionL, positionR), 1.7)
        + 0.5 * laserIntensity(uv, fd_reye_1, fd_reye_2, 0.6, distance(positionL, positionR), 1.7)
    );

    float4 px = image.Sample(textureSampler, uv);

    if (intensityOfLaser > 0.00000) {
        float4 laserColor;
        laserColor.r = clamp(intensityOfLaser*1.00, 0.0, 1.0);
        laserColor.g = clamp(intensityOfLaser*0.12, 0.0, 0.85);
        laserColor.b = clamp(intensityOfLaser*0.10, 0.0, 0.8);
        laserColor.a = clamp(intensityOfLaser*0.60, 0.0, 1.0);
        //laserColor.a = 1.0;
        px += laserColor;
        px.r = clamp(px.r, 0.0, 1.0);
        px.g = clamp(px.g, 0.0, 1.0);
        px.b = clamp(px.b, 0.0, 1.0);
        px.a = clamp(px.a, 0.0, 1.0);
    }

    float3 px_interm_yuv = rgb2yuv(px_interm.rgb);
    float3 px_dest = lerp(
        px.rgb,
        px_interm.rgb * 0.7 + px.rgb * 0.7,
        px_interm.a * px_interm_yuv.x
    );

    float audio_effect_level2 = audio_effect_level();
    float audio_level_mult = audio_effect_level2;

    return float4(px_dest, px.a);
}
//----------------------------------------------------------------------------------------------------------------------

float4 EffectLinear(float2 uv) {
    if (current_step == 0) {
        return EffectLinear__step0(uv);
    }
    else if (current_step == 1) {
        return EffectLinear__step1(uv);
    }
    else if (current_step == 2) {
        return EffectLinear__step2(uv);
    }
    else {
        return EffectLinear__step3(uv);
    }
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
