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
    PSIn.PosH = float4(VSIn.PosL, 1.0);
    PSIn.Color = VSIn.Color;
}