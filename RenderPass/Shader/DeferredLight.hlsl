#include "Common.hlsl"
#include "BRDF.hlsl"
#include "IBLLighting.hlsl"
#include "Tonemapping.hlsl"
#include "VSM.hlsl"
struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    int type;
};

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

cbuffer ShadowConstants : register(b1)
{
    float4x4 g_LightVP;
    float g_ShadowNearZ;
    float g_ShadowFarZ;
}

StructuredBuffer<Light> g_Lights : register(t0, space1);
Texture2D<float4>    g_PositionHit : register(t0);
Texture2D<float4>    g_AlbedoRoughness : register(t1);
Texture2D<float4>    g_NormalMetallic : register(t2);
Texture2D<float4>    g_MotionLinearZ : register(t3);
Texture2D<uint>      g_ObjectID : register(t4);

Texture2D    g_BRDFLUT : register(t5);
TextureCube  g_IrradianceEnvMap : register(t6);
TextureCube  g_PrefilteredEnvMap : register(t7);

Texture2D g_ShadowMap : register(t8);

SamplerState g_samLinearClamp        : register(s0);

void VS(in  uint VertexId : SV_VertexID,
    out float4 Pos : SV_Position)
{
    float2 PosXY[3];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +3.0);
    PosXY[2] = float2(+3.0, -1.0);

    float2 f2XY = PosXY[VertexId];
    Pos = float4(f2XY, 1.0, 1.0);
}

float3 ComputeDirectionalLight(Light L, float3 normal, float3 view, inout BRDFData BRDF)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = normalize(-L.Direction);

    float3 lightStrength = L.Strength;

    float NoL = saturate(dot(normal, lightVec));

    return DirectLightBRDF(lightVec, view, normal, BRDF) * lightStrength * NoL;
}

void PS(in  float4 Pos     : SV_Position,
    out float4 Color : SV_Target)
{
    int2 ipos = Pos.xy;

    float hit = g_PositionHit.Load(int3(ipos, 0)).w;
    if (hit <= 0)
    {
        Color = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float3 position = g_PositionHit.Load(int3(ipos, 0)).xyz;
    float3 normal = g_NormalMetallic.Load(int3(ipos, 0)).xyz;
    float3 view = normalize(g_CameraPos - position);
    float vis = CalculateShadow(g_ShadowMap, g_LightVP, position, g_ShadowNearZ, g_ShadowFarZ);

    BRDFData data = GetBRDF(g_AlbedoRoughness, g_NormalMetallic, ipos);


    Light L = g_Lights[0];
    float4 litColor = float4(0.0, 0.0, 0.0, 1.0);

    litColor.rgb += ComputeDirectionalLight(L, normal, view, data) * vis;
    
    //IBL
    IBLContribution IBLContrib = IBL(normal, view, g_PrefilteredEnvMipLevels, data, g_BRDFLUT, g_IrradianceEnvMap, g_PrefilteredEnvMap, g_samLinearClamp);
    litColor.rgb += (IBLContrib.Diffuse + IBLContrib.Specular) * 0.5;

    //ToneMapping
    ToneMappingAttribs TMAttribs;
    TMAttribs.fMiddleGray = 0.18;
    TMAttribs.fWhitePoint = 3.0;
    TMAttribs.fLuminanceSaturation = 1.0;
    litColor.rgb = ToneMap(litColor.rgb, TMAttribs, 0.3);

    Color = litColor;
}
