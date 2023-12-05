cbuffer LightConstants: register(b0)
{
    float4x4 g_LightVP;
};

cbuffer MeshConstants: register(b1)
{
    float4x4 g_World;
    float4x4 g_WorldInvertTran;
};

struct VSInput
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION1;
};

struct PSOutput
{
    float2 Color : SV_TARGET0;
};

void VS(VSInput VSIn, out PSInput PSIn)
{
    float4 posW = mul(float4(VSIn.PosL, 1.0), g_World);
    PSIn.PosW = posW.xyz;
    PSIn.PosH = mul(posW, g_LightVP);
}

void PS(in PSInput PSIn, out PSOutput PSOut)
{
    PSOut.Color = float2(PSIn.PosH.z, PSIn.PosH.z * PSIn.PosH.z);
}
