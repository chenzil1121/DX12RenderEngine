#include "Common.hlsl"
#include "Tonemapping.hlsl"

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

TextureCube  g_EnvMap : register(t0);
SamplerState g_samLinearWrap        : register(s0);

//��NDC�ռ乹��һ��������������������Ļ�����ٽ������Ļ��������VP��仯������ռ�,
//���������������ϵ���������Ļ�������������cubemap��
void VS(in  uint   VertexId : SV_VertexID,
    out float4 Pos : SV_Position,
    out float4 ClipPos : CLIP_POS)
{
    float2 PosXY[3];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +3.0);
    PosXY[2] = float2(+3.0, -1.0);

    float2 f2XY = PosXY[VertexId];
    //NDC��zΪ1������λ��Զƽ�棬�����ڵ��κ���Ⱦ�õ�����
    Pos = float4(f2XY, 1.0, 1.0);
    ClipPos = Pos;
}

void PS(in  float4 Pos     : SV_Position,
    in  float4 ClipPos : CLIP_POS,
    out float4 Color : SV_Target)
{
    float4 WorldPos = mul(ClipPos, g_ViewProjInvert);
    float3 Direction = WorldPos.xyz / WorldPos.w - g_CameraPos;
    float4 EnvRadiance = g_EnvMap.Sample(g_samLinearWrap, Direction);
    
    ToneMappingAttribs TMAttribs;
    TMAttribs.fMiddleGray = 0.18;
    TMAttribs.fWhitePoint = 3.0;
    TMAttribs.fLuminanceSaturation = 1.0;
    EnvRadiance.rgb = ToneMap(EnvRadiance.rgb, TMAttribs, 0.3);
    Color = float4(EnvRadiance.rgb, EnvRadiance.a);
}