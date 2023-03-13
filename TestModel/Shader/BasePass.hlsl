#include "BRDF.hlsl"
#include "IBLLighting.hlsl"
#include "Tonemapping.hlsl"
struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    int type;
};
StructuredBuffer<Light> g_Lights : register(t0, space1);

Texture2D    g_BaseColorMap : register(t0);
Texture2D    g_RoughnessMetallicMap : register(t1);
Texture2D    g_NormalMap : register(t2);

Texture2D    g_BRDFLUT : register(t3);
TextureCube  g_IrradianceEnvMap : register(t4);
TextureCube  g_PrefilteredEnvMap : register(t5);


SamplerState g_samLinearWrap        : register(s0);
SamplerState g_samLinearClamp        : register(s1);

cbuffer MeshConstants: register(b0)
{
    float4x4 g_World;
    float4x4 g_WorldInvertTran;
};

cbuffer PassConstants: register(b1)
{
    float4x4 g_ViewProj;
    float4x4 g_ViewProjInvert;
    float3 g_CameraPos;
    float pad0;
    int3 g_FirstLightIndex;
    int g_PrefilteredEnvMipLevels;
    int g_DebugViewType;
};

struct VSInput
{
    float3 PosL   : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PSInput
{
    float4 PosH   : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, float3 normal, float3 view, inout BRDFData BRDF)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = normalize(-L.Direction);

    float3 lightStrength = L.Strength;

    float NoL = saturate(dot(normal, lightVec));

    return DirectLightBRDF(lightVec, view, normal, BRDF) * lightStrength * NoL;
}

void VS(VSInput VSIn, out PSInput PSIn)
{
    float4 poW = mul(float4(VSIn.PosL, 1.0), g_World);
    PSIn.PosW = poW.xyz;
    PSIn.PosH = mul(poW, g_ViewProj);

    PSIn.NormalW = normalize(mul(VSIn.NormalL, float3x3(g_WorldInvertTran[0].xyz, g_WorldInvertTran[1].xyz, g_WorldInvertTran[2].xyz)));
    PSIn.TexC = VSIn.TexC;
}

void PS(in  PSInput  PSIn, out PSOutput PSOut)
{
    float4 baseColor = g_BaseColorMap.Sample(g_samLinearWrap, PSIn.TexC);
    float4 roughnessMetallic = g_RoughnessMetallicMap.Sample(g_samLinearWrap, PSIn.TexC);
    float3 normalMap = g_NormalMap.Sample(g_samLinearWrap, PSIn.TexC).rgb * float3(2.0, 2.0, 2.0) - float3(1.0, 1.0, 1.0);

    // We have to compute gradients in uniform flow control to avoid issues with perturbed normal
    float3 dWorldPos_dx = ddx(PSIn.PosW);
    float3 dWorldPos_dy = ddy(PSIn.PosW);
    float2 dUV_dx = ddx(PSIn.TexC);
    float2 dUV_dy = ddy(PSIn.TexC);
    float3 normal = normalize(PSIn.NormalW);

    float3 perturbedNormal = TransformTangentSpaceNormalGrad(dWorldPos_dx, dWorldPos_dy, dUV_dx, dUV_dy, normal, normalMap);
    //float3 perturbedNormal = normal;

    BRDFData BRDF;
    BRDF.baseColor = baseColor.rgb;
    BRDF.roughness = saturate(roughnessMetallic.g);
    BRDF.metallic = saturate(roughnessMetallic.b);
    BRDF.DiffuseColor = float3(0.0, 0.0, 0.0);
    BRDF.F0 = 0.0;
    BRDF.F90 = 0.0;

    float3 view = normalize(g_CameraPos - PSIn.PosW);

    float4 litColor = float4(0.0, 0.0, 0.0, baseColor.a);

    //Directional
    for (int i = 0; i < g_FirstLightIndex.x; i++)
    {
        litColor.rgb += ComputeDirectionalLight(g_Lights[i], perturbedNormal, view, BRDF);
    }

    //IBL
    IBLContribution IBLContrib = IBL(perturbedNormal, view, g_PrefilteredEnvMipLevels, BRDF, g_BRDFLUT, g_IrradianceEnvMap, g_PrefilteredEnvMap, g_samLinearClamp);
    litColor.rgb += IBLContrib.Diffuse + IBLContrib.Specular;

    //ToneMapping
    ToneMappingAttribs TMAttribs;
    TMAttribs.fMiddleGray = 0.18;
    TMAttribs.fWhitePoint = 3.0;
    TMAttribs.fLuminanceSaturation = 1.0;
    litColor.rgb = ToneMap(litColor.rgb, TMAttribs, 0.3);

    PSOut.Color = float4(litColor.rgb, litColor.a);
    if (g_DebugViewType != 0)
    {
        switch (g_DebugViewType)
        {
        case 1:PSOut.Color.rgb = BRDF.baseColor;
            break;
        case 2:PSOut.Color = float4(baseColor.a, baseColor.a, baseColor.a, 1.0);
            break;
        case 3:PSOut.Color.rgb = sRGB_Linear(normalMap.rgb);
            break;
        case 4:break;
        case 5:break;
        case 6:PSOut.Color.rgb = sRGB_Linear(BRDF.metallic * float3(1.0, 1.0, 1.0));
            break;
        case 7:PSOut.Color.rgb = sRGB_Linear(BRDF.roughness * float3(1.0, 1.0, 1.0));
            break;
        case 8:PSOut.Color.rgb = BRDF.DiffuseColor;
            break;
        case 9:PSOut.Color.rgb = BRDF.F0;
            break;
        case 10:PSOut.Color.rgb = BRDF.F90;
            break;
        case 11:PSOut.Color.rgb = sRGB_Linear(normal);
            break;
        case 12:PSOut.Color.rgb = sRGB_Linear(perturbedNormal);
            break;
        case 13:PSOut.Color.rgb = dot(perturbedNormal, view) * float3(1.0, 1.0, 1.0);
            break;
        case 14:PSOut.Color.rgb = IBLContrib.Diffuse;
            break;
        case 15:PSOut.Color.rgb = IBLContrib.Specular;
            break;
        }
    }
}