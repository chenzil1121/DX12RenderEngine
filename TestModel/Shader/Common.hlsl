#ifndef COMMON_HLSL
#define COMMON_HLSL
const static float PI = 3.14159265359;

float3 sRGB_Linear(float3 x)
{
	return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

float3 Linear_sRGB(float3 x)
{
	return float3(pow(x[0], 0.45), pow(x[1], 0.45), pow(x[2], 0.45));
}

float3x3 InverseTranspose3x3(float3x3 M)
{
    // Note that in HLSL, M_t[0] is the first row, while in GLSL, it is the
    // first column. Luckily, determinant and inverse matrix can be equally
    // defined through both rows and columns.
    float det = dot(cross(M[0], M[1]), M[2]);
    float3x3 adjugate = float3x3(cross(M[1], M[2]),
        cross(M[2], M[0]),
        cross(M[0], M[1]));
    return adjugate / det;
}

// Transforms the normal from tangent space to world space using the
// position and UV derivatives.
float3 TransformTangentSpaceNormalGrad(
    in float3 dPos_dx,     // Position dx derivative
    in float3 dPos_dy,     // Position dy derivative
    in float2 dUV_dx,      // Normal map UV coordinates dx derivative
    in float2 dUV_dy,      // Normal map UV coordinates dy derivative
    in float3 MacroNormal, // Macro normal, must be normalized
    in float3 TSNormal,     // Tangent-space normal
    in bool HasUV
)

{
    float3 n = MacroNormal;
    if (HasUV)
    {
        float3 t = (dUV_dy.y * dPos_dx - dUV_dx.y * dPos_dy) / (dUV_dx.x * dUV_dy.y - dUV_dy.x * dUV_dx.y);
        t = normalize(t - n * dot(n, t));

        float3 b = normalize(cross(t, n));

        float3x3 tbn = float3x3(t, b, n);
        n = normalize(mul(TSNormal, tbn));
    }

    return n;
}
#endif