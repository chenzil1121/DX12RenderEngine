cbuffer Constants: register(b0)
{
    float4x4 g_WorldViewProj;
};

struct PSInput
{
    float4 PosH   : SV_POSITION;
    float4 Color : COLOR;
};

struct VSInput
{
    float3 PosL   : POSITION;
    float4 Color : COLOR;
};

void main(VSInput VSIn, out PSInput PSIn)
{
    PSIn.PosH = mul(float4(VSIn.PosL, 1.0), g_WorldViewProj);

    PSIn.Color = VSIn.Color;
}