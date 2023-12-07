#include "Common.hlsl"
#include "Tonemapping.hlsl"
#include "BRDF.hlsl"

//Root 0
Texture2D<float4>    g_PositionHit : register(t0);
Texture2D<float4>    g_AlbedoRoughness : register(t1);
Texture2D<float4>    g_NormalMetallic : register(t2);
Texture2D<float4>    g_MotionLinearZ : register(t3);
//Root 1
Texture2D<float4> g_FilteredShadow : register(t4);
//Root 2
Texture2D<float4> g_FilteredReflection : register(t5);
//Root 3
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
//Root 4
struct PointLight
{
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	int type;
}; ConstantBuffer<PointLight> g_PointLight : register(b1);


float3 DoPbrPointLight(PointLight light, float3 N, float3 V, float3 P, float3 albedo, float roughness, float metallic)
{
	float3 F0 = F0_DIELECTRIC.rrr;
	F0 = lerp(F0, albedo, metallic);
	float reflectance = max(max(F0.r, F0.g), F0.b);
	float3 F90 = clamp(reflectance * 50.0, 0.0, 1.0) * float3(1.0, 1.0, 1.0);
	float3 diffuseColor = (float3(1.0, 1.0, 1.0) - F0_DIELECTRIC.rrr) * (1.0 - metallic) * albedo;
	
	float3 L = normalize(light.Position - P);
	float3 H = normalize(V + L);
	float NoL = clamp(dot(N, L), 0.0, 1.0);
	float NoV = clamp(dot(N, V), 0.0, 1.0);
	float NoH = clamp(dot(N, H), 0.0, 1.0);
	float LoH = clamp(dot(L, H), 0.0, 1.0);
	float VoH = clamp(dot(V, H), 0.0, 1.0);

	float AlphaRoughness = roughness * roughness;

	if (NoL <= 0.0 && NoV <= 0.0)
		return float3(0.0, 0.0, 0.0);

	float D = NormalDistribution_GGX(NoH, AlphaRoughness);
	float G = SmithGGXVisibilityCorrelated(NoL, NoV, AlphaRoughness);
	float3 F = SchlickReflection(VoH, F0, F90);

	float3 SpecularBRDF = D * F * G;
	float3 DiffuseBRDF = LambertianDiffuse(diffuseColor);

	return ((1.0 - F) * DiffuseBRDF + SpecularBRDF) * light.Strength * NoL;
}

//从NDC空间构造一个三角形来覆盖整个屏幕区域，再将这个屏幕区域利用VP逆变化到世界空间,
void VS(in  uint   VertexId : SV_VertexID,
    out float4 Pos : SV_Position)
{
    float2 PosXY[3];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +3.0);
    PosXY[2] = float2(+3.0, -1.0);

    float2 f2XY = PosXY[VertexId];
    //NDC下z为1代表它位于远平面，不会遮挡任何渲染好的物体
    Pos = float4(f2XY, 1.0, 1.0);
}

void PS(in  float4 Pos     : SV_Position,
    out float4 Color : SV_Target)
{
	int2 texCoord = (int2) Pos.xy;

	float hit = g_PositionHit.Load(int3(texCoord.x, texCoord.y, 0)).w;
	float3 position = g_PositionHit.Load(int3(texCoord.x, texCoord.y, 0)).xyz;
	float3 albedo = g_AlbedoRoughness.Load(int3(texCoord.x, texCoord.y, 0)).xyz;
	float roughness = g_AlbedoRoughness.Load(int3(texCoord.x, texCoord.y, 0)).w;
	float metallic = g_NormalMetallic.Load(int3(texCoord.x, texCoord.y, 0)).w;
	float3 normal = g_NormalMetallic.Load(int3(texCoord.x, texCoord.y, 0)).xyz;

	float shadow = g_FilteredShadow.Load(int3(texCoord.x, texCoord.y, 0)).x;
	float ao = g_FilteredShadow.Load(int3(texCoord.x, texCoord.y, 0)).y;
	float3 reflectivity = g_FilteredReflection.Load(int3(texCoord.x, texCoord.y, 0)).xyz;
	
	if (hit == 0.0)
	{
		Color = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	float3 P = position;
	float3 V = normalize(g_CameraPos - position);
	float3 N = normalize(normal);

	BRDFData BRDF = GetBRDF(
		albedo,
		roughness,
		metallic
	);

	//Direct + Indirect
	float3 ColorSum;
	float3 ColorDirect = DoPbrPointLight(g_PointLight, N, V, P, albedo, roughness, metallic);
	float3 ColorIndirect = reflectivity * albedo;

	ColorSum = shadow * ColorDirect + ao * ColorIndirect;
	//ColorSum = ColorIndirect;

	ToneMappingAttribs TMAttribs;
	TMAttribs.fMiddleGray = 0.18;
	TMAttribs.fWhitePoint = 3.0;
	TMAttribs.fLuminanceSaturation = 1.0;
	ColorSum = ToneMap(ColorSum, TMAttribs, 0.3);
	Color = float4(ColorSum, 1.0);
}