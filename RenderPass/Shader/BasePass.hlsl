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
StructuredBuffer<Light> g_Lights : register(t0, space1);

Texture2D    g_BaseColorMap : register(t0);
Texture2D    g_RoughnessMetallicMap : register(t1);
Texture2D    g_NormalMap : register(t2);

Texture2D    g_BRDFLUT : register(t3);
TextureCube  g_IrradianceEnvMap : register(t4);
TextureCube  g_PrefilteredEnvMap : register(t5);
Texture2D   g_ShadowMap : register(t6);


SamplerState g_samLinearWrap        : register(s0);
SamplerState g_samLinearClamp        : register(s1);

cbuffer MeshConstants: register(b0)
{
    float4x4 g_World;
    float4x4 g_PreWorld;
    float4x4 g_WorldInvertTran;
    uint g_ID;
};

cbuffer PassConstants: register(b1)
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

cbuffer MaterialConstants: register(b2)
{
    float4 g_BaseColorFactor;
    float g_RoughnessFactor;
    float g_MetallicFactor;
    bool g_HasUV;
}

cbuffer ShadowConstants : register(b3)
{
    float4x4 g_LightVP;
    float g_ShadowNearZ;
    float g_ShadowFarZ;
}

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

    // GetBRDF
    BRDFData BRDF = GetBRDF(g_BaseColorMap, 
        g_RoughnessMetallicMap, 
        g_NormalMap, 
        PSIn.TexC,
        g_samLinearWrap,
        g_BaseColorFactor,
        g_RoughnessFactor,
        g_MetallicFactor
    );

    float3 view = normalize(g_CameraPos - PSIn.PosW);

    float4 litColor = float4(0.0, 0.0, 0.0, BRDF.baseColor.a);

    //Directional
    /*for (int i = 0; i < g_FirstLightIndex.x; i++)
    {
        litColor.rgb += ComputeDirectionalLight(g_Lights[i], perturbedNormal, view, BRDF);
    }*/

    //MainLight(Directional)
    float vis = CalculateShadow(g_ShadowMap, g_LightVP, PSIn.PosW, g_ShadowNearZ, g_ShadowFarZ);
    litColor.rgb += ComputeDirectionalLight(g_Lights[0], perturbedNormal, view, BRDF) * vis;
   

    //IBL
    IBLContribution IBLContrib = IBL(perturbedNormal, view, g_PrefilteredEnvMipLevels, BRDF, g_BRDFLUT, g_IrradianceEnvMap, g_PrefilteredEnvMap, g_samLinearClamp);
    litColor.rgb += (IBLContrib.Diffuse + IBLContrib.Specular) * 0.5;

    //ToneMapping
    ToneMappingAttribs TMAttribs;
    TMAttribs.fMiddleGray = 0.18;
    TMAttribs.fWhitePoint = 3.0;
    TMAttribs.fLuminanceSaturation = 1.0;
    litColor.rgb = ToneMap(litColor.rgb, TMAttribs, 0.3);

    PSOut.Color = float4(litColor.rgb, litColor.a);
}