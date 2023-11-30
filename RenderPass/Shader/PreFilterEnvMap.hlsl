/*
* PreFilterEnvMap和PreIrradiance的最主要区别是
* PreIrradiance是整个半球上均匀采样的积分用于Diffuse Light
* PreFilterEnvMap是半球上以N=V为前提的Specular区域的Light重要性采样的积分
* Specular indirect Light的核心在于给一个V(相机视角向量)，需要知道对应Specular BRDF区域的间接入射光的总和
* 这里有一个近似，给定物体的BRDF在半球上形状是一样的，偏离法线45°观察和偏离法线0°(从法线角度观察)的BRDF形状是一样的！
* 因此可以在预计算PreFilterEnvMap的时候，我们可以把N=V当作前提。这么做的好处是，实时渲染间接光部分时，我们只知道N和V。
* 但根据N和V利用reflect函数可以得到R，这个R相当于预计算时候的N，因此它可以查询到对应间接光的积分和。
* 预计算时候从一个Cube向整个球面采样了许多N相当于采样了许多R，因此球面各个R都可以查到自己的间接光的积分和。
*/

#include<Common.hlsl>

cbuffer cbTransform: register(b0)
{
    float4x4 g_Rotation;
    float g_Roughness;
    uint g_NumSamples;
    float2 Unuesd;
}

TextureCube  g_EnvMap : register(t0);
SamplerState g_samLinearClamp : register(s0);

void VS(in uint VertexId     : SV_VertexID,
    out float4 Pos : SV_Position,
    out float3 WorldPos : WORLD_POS)
{
    //不用装配顶点，使用D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP形成两个三角形
    //这两个三角形覆盖了全屏
    float2 PosXY[4];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +1.0);
    PosXY[2] = float2(+1.0, -1.0);
    PosXY[3] = float2(+1.0, +1.0);
    Pos = float4(PosXY[VertexId], 1.0, 1.0);
    //默认是+Z面，通过6个不同的旋转矩阵，形成一个原点为中心的包围立方体，从原点(相机位置)到6个面的向量正好覆盖了整体球体的法线采样情况
    //这里IBL假设物体上每一个面片都位于CubeMap形成球体的中心
    float4 f4WorldPos = mul(Pos, g_Rotation);
    WorldPos = f4WorldPos.xyz / f4WorldPos.w;
}

float2 Hammersley2D(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N)
{
    float a = Roughness * Roughness;
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

void PS(in float4  Pos      : SV_Position,
    in float3  WorldPos : WORLD_POS,
    out float4 Color : SV_Target)
{
    float3 R = normalize(WorldPos);
    float3 N = R;
    float3 V = R;
    float3 PrefilteredColor = float3(0.0, 0.0, 0.0);

    float TotalWeight = 0.0;
    for (uint i = 0u; i < g_NumSamples; i++)
    {
        float2 Xi = Hammersley2D(i, g_NumSamples);
        float3 H = ImportanceSampleGGX(Xi, g_Roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        float NoL = clamp(dot(N, L), 0.0, 1.0);
        float VoH = clamp(dot(V, H), 0.0, 1.0);
        if (NoL > 0.0 && VoH > 0.0)
        {
            PrefilteredColor += g_EnvMap.Sample(g_samLinearClamp, L).rgb * NoL;
            TotalWeight += NoL;
        }
    }
    Color.rgb = PrefilteredColor / TotalWeight;
    Color.a = 1.0;
}

