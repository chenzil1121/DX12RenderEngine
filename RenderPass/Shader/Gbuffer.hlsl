#include "Common.hlsl"

cbuffer PassConstants: register(b0)
{
    float4x4 g_ViewProj;
    float4x4 g_ViewProjInvert;
    float4x4 g_PreViewProj;
    float4x4 g_View;
    float4x4 g_Proj;
    float3 g_CameraPos;
    float g_NearZ;
    float g_FarZ;
    int2 g_Dim;
    int g_PrefilteredEnvMipLevels;
    int3 g_FirstLightIndex;
};

cbuffer MeshConstants: register(b1)
{
    float4x4 g_World;
    float4x4 g_PreWorld;
    float4x4 g_WorldInvertTran;
    uint g_ID;
};

SamplerState g_samLinearWrap : register(s0);

Texture2D    g_BaseColorMap : register(t0);
Texture2D    g_RoughnessMetallicMap : register(t1);
Texture2D    g_NormalMap : register(t2);

cbuffer MaterialConstants: register(b2)
{
    float4 g_BaseColorFactor;
    float g_RoughnessFactor;
    float g_MetallicFactor;
    bool g_HasUV;
}

struct VSInput
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float4 PrePosH : POSITION0;
    float3 PosW : POSITION1;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    float  LinearZ : LINEARZ;
};

struct PSOutput
{
    float4 PositionAndHitness : SV_TARGET0;
    float4 AlbedoAndRoughness : SV_TARGET1;
    float4 NormalAndMetallic : SV_TARGET2;
    float4 MotionLinearZ : SV_TARGET3;
    uint ObjectID : SV_TARGET4;
};

float LinearZ(float4 outPosition)
{
    float2 projectionConstants;
    projectionConstants.x = g_FarZ / (g_FarZ - g_NearZ);
    projectionConstants.y = (-g_FarZ * g_NearZ) / (g_FarZ - g_NearZ);

    float depth = outPosition.z / outPosition.w;
    float linearZ = projectionConstants.y / (depth - projectionConstants.x);
    return linearZ;
}

float2 computeMotionVector(float4 prevPosH, const int2 ipos)
{
    float2 pixelPos = ipos + float2(0.5, 0.5); // Current sample in pixel coords.
    float2 prevCrd = prevPosH.xy / prevPosH.w * float2(0.5, -0.5) + float2(0.5, 0.5);
    float2 normalizedCrd = pixelPos / g_Dim;
    return prevCrd - normalizedCrd;
}

void VS(VSInput VSIn, out PSInput PSIn)
{
    float4 posW = mul(float4(VSIn.PosL, 1.0), g_World);
    PSIn.PosW = posW.xyz;
    PSIn.PosH = mul(posW, g_ViewProj);

    PSIn.NormalW = normalize(mul(VSIn.NormalL, float3x3(g_WorldInvertTran[0].xyz, g_WorldInvertTran[1].xyz, g_WorldInvertTran[2].xyz)));
    PSIn.TexC = VSIn.TexC;
    PSIn.LinearZ = LinearZ(PSIn.PosH);
    float4 prePosW = mul(float4(VSIn.PosL, 1.0), g_PreWorld);
    PSIn.PrePosH = mul(prePosW, g_PreViewProj);
}


void PS(in PSInput  PSIn, out PSOutput PSOut)
{
    float3 position = PSIn.PosW;
    float3 albedo = sRGB_Linear(g_BaseColorMap.Sample(g_samLinearWrap, PSIn.TexC).rgb * g_BaseColorFactor.rgb);

    float3 normalMap = g_NormalMap.Sample(g_samLinearWrap, PSIn.TexC).rgb * float3(2.0, 2.0, 2.0) - float3(1.0, 1.0, 1.0);
    // We have to compute gradients in uniform flow control to avoid issues with perturbed normal
    float3 dWorldPos_dx = ddx(PSIn.PosW);
    float3 dWorldPos_dy = ddy(PSIn.PosW);
    float2 dUV_dx = ddx(PSIn.TexC);
    float2 dUV_dy = ddy(PSIn.TexC);
    float3 normal = normalize(PSIn.NormalW);

    float3 perturbedNormal = TransformTangentSpaceNormalGrad(dWorldPos_dx,
        dWorldPos_dy,
        dUV_dx,
        dUV_dy,
        normal,
        normalMap,
        g_HasUV
    );
    

    float hitness = 1.0;
    float roughness = g_RoughnessMetallicMap.Sample(g_samLinearWrap, PSIn.TexC).g * g_RoughnessFactor;
    float metallic = g_RoughnessMetallicMap.Sample(g_samLinearWrap, PSIn.TexC).b * g_MetallicFactor;

    int2 ipos = int2(PSIn.PosH.xy);
    float2 motion = computeMotionVector(PSIn.PrePosH, ipos);

    PSOut.PositionAndHitness = float4(position, hitness);
    PSOut.AlbedoAndRoughness = float4(albedo, roughness);
    PSOut.NormalAndMetallic = float4(perturbedNormal, metallic);
    PSOut.MotionLinearZ = float4(motion, PSIn.LinearZ, 1.0f);
    PSOut.ObjectID = g_ID;
}
