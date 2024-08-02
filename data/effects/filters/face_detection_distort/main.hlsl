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
uniform texture2d fd_points_tex;
uniform float screen_distortion;

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

float rand2(float2 co){
	float v = sin(dot(co, float2(12.9898, 78.233))) * 43758.5453;
	return fract(v);
}
float rand(float a, float b) {
	return rand2(float2(a, b));
}

float angleBetween(float2 v1, float2 v2) {
    return atan2(v2.y - v1.y, v2.x - v1.x);
}

float2 rotate(float2 uvLocal, float angle) {
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return float2(
        uvLocal.x * cosAngle + uvLocal.y * sinAngle,
        -uvLocal.x * sinAngle + uvLocal.y * cosAngle
    );
}
//----------------------------------------------------------------------------------------------------------------------

float4 grossir(float2 uv, float2 center, float x, float amount, float width) {
    uv -= center;
    uv.x *= vpixel/upixel;
    float r = distance(uv, float2(0.0, 0.0));

    if (x > 1) {
        uv.x /= vpixel/upixel;
        uv += center;
        return image.Sample(textureSampler, uv);
    }

    float y_m = amount;
    float x_m = width*0.5;
    float2 direction = normalize(uv);

    float displacement = 0.0;

    // mode x³
    float a = -amount*2.0;
    float c = -a/2.0;
    float b = (-3.0*a)/4.0 - c;
    displacement = a*x*x*x + b*x*x + c*x;
    displacement = max(0.0, displacement);

    uv.y -= displacement*width * direction.y;
    uv.x -= displacement*width * direction.x;
    uv.x /= vpixel/upixel;
    uv += center;

    return image.Sample(textureSampler, uv);
}
//----------------------------------------------------------------------------------------------------------------------

float arcEllipse(float2 A, float2 B, float2 center, float2 uv) {
    // Convert to an orthonormal coordinate system, as we will rotate things
    float aspectRatio = vpixel/upixel;
    A.x *= aspectRatio;
    B.x *= aspectRatio;
    center.x *= aspectRatio;
    uv.x *= aspectRatio;

    // Translate uv to the ellipse's local coordinates
    float2 ALocal = A - center;
    float2 BLocal = B - center;
    float2 uvLocal = uv - center;

    // Calculate the semi-major and semi-minor axes
    float2 minorAxis;
    float2 majorAxis;
    float majorAxisLength;
    float minorAxisLength;
    bool AIsMajor = length(ALocal) > length(BLocal);
    if (AIsMajor) {
        majorAxis = ALocal;
        majorAxisLength = length(majorAxis);

        // Calculate the semi-minor axis using point B
        float dotProduct = dot(normalize(majorAxis), normalize(BLocal));
        minorAxisLength = length(BLocal) * sqrt(1.0 - dotProduct * dotProduct);
    }
    else {
        minorAxis = ALocal;
        minorAxisLength = length(minorAxis);

        // Calculate the semi-minor axis using point B
        float dotProduct = dot(normalize(minorAxis), normalize(BLocal));
        majorAxis = BLocal * sqrt(1.0 - dotProduct * dotProduct);
        majorAxisLength = length(majorAxis);
    }

    // Rotate uvLocal to align with the ellipse's major and minor axes
    float angle = atan2(ALocal.y, ALocal.x);

    float2 uvRotated = rotate(uvLocal, angle);
    float2 ARotated = rotate(ALocal, angle);
    float2 BRotated = rotate(BLocal, angle);

    // Check if the point is inside the ellipse
    float ellipseUvPosition;
    if (AIsMajor) {
        ellipseUvPosition = (uvRotated.x * uvRotated.x) / (majorAxisLength * majorAxisLength) +
                            (uvRotated.y * uvRotated.y) / (minorAxisLength * minorAxisLength);
    }
    else {
        ellipseUvPosition = (uvRotated.y * uvRotated.y) / (majorAxisLength * majorAxisLength) +
                            (uvRotated.x * uvRotated.x) / (minorAxisLength * minorAxisLength);
    }

    if (uvRotated.y > 0) {
        return sqrt(ellipseUvPosition);
    }
    else {
        return -sqrt(ellipseUvPosition);
    }
}

float4 EffectLinear_real(float2 uv)
{
    float2 face_tip = fd_points_tex.Sample(pointsSampler, float2((1 + 0.5)/468.0, 0)).xy;
    float2 face_center = fd_points_tex.Sample(pointsSampler, float2((197 + 0.5)/468.0, 0)).xy;
    float2 face_top = fd_points_tex.Sample(pointsSampler, float2((10 + 0.5)/468.0, 0)).xy;
    float2 face_bottom = fd_points_tex.Sample(pointsSampler, float2((152 + 0.5)/468.0, 0)).xy;

    float2 face_left1 = fd_points_tex.Sample(pointsSampler, float2((234 + 0.5)/468.0, 0)).xy;
    float2 face_left2 = fd_points_tex.Sample(pointsSampler, float2((227 + 0.5)/468.0, 0)).xy;
    float2 face_left3 = fd_points_tex.Sample(pointsSampler, float2((116 + 0.5)/468.0, 0)).xy;
    float2 face_left  = fd_points_tex.Sample(pointsSampler, float2((117 + 0.5)/468.0, 0)).xy;
    if (face_left.x > face_left1.x) {
        face_left = face_left1;
    }
    if (face_left.x > face_left2.x) {
        face_left = face_left2;
    }
    if (face_left.x > face_left3.x) {
        face_left = face_left3;
    }
    if (face_left.x > face_tip.x) {
        face_left = face_tip;
    }

    float2 face_right1 = fd_points_tex.Sample(pointsSampler, float2((454 + 0.5)/468.0, 0)).xy;
    float2 face_right2 = fd_points_tex.Sample(pointsSampler, float2((447 + 0.5)/468.0, 0)).xy;
    float2 face_right3 = fd_points_tex.Sample(pointsSampler, float2((345 + 0.5)/468.0, 0)).xy;
    float2 face_right  = fd_points_tex.Sample(pointsSampler, float2((346 + 0.5)/468.0, 0)).xy;
    if (face_right.x < face_right1.x) {
        face_right = face_right1;
    }
    if (face_right.x < face_right2.x) {
        face_right = face_right2;
    }
    if (face_right.x < face_right3.x) {
        face_right = face_right3;
    }
    if (face_right.x < face_tip.x) {
        face_right = face_tip;
    }

    float ellipseVal = 0.0;

    float4 ellipseColor = float4(0.0, 0.0, 0.0, 1.0);

    face_center = (face_top + face_bottom) / 2.0;

    float ellipseRightVal = arcEllipse(face_top, face_right, face_center, uv);
    float ellipseLeftVal = arcEllipse(face_top, face_left, face_center, uv);
    if (0 <= ellipseRightVal && ellipseRightVal <= 1.0) {
        ellipseVal = ellipseRightVal;
        ellipseColor = float4(0.0, ellipseRightVal, 0.0, 1.0);
    }
    else if (-1.0 <= ellipseLeftVal && ellipseLeftVal <= 0.0) {
        ellipseVal = -ellipseLeftVal;
        ellipseColor = float4(-ellipseLeftVal, 0.0, 0.0, 1.0);
    }

    float width = distance(face_center, face_top);

    float4 px = grossir(uv, face_center.xy, ellipseVal, screen_distortion, width);
    return px;
}
//----------------------------------------------------------------------------------------------------------------------
float4 EffectLinear(float2 uv) {
    float4 px_orig = image.Sample(textureSampler, uv);

    return EffectLinear_real(uv);
}
//----------------------------------------------------------------------------------------------------------------------

// You probably don't want to change anything from this point.

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
