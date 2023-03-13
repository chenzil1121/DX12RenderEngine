struct PSInput
{
    float4 PosH   : SV_POSITION;
    float4 Color : COLOR;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
    out PSOutput PSOut)
{
    PSOut.Color = PSIn.Color;
}