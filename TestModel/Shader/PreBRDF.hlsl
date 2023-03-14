#include"BRDF.hlsl"
void VS(in  uint   VertexId : SV_VertexID,
    out float4 Pos : SV_Position,
    out float4 ClipPos : CLIP_POS)
{
    float2 PosXY[3];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +3.0);
    PosXY[2] = float2(+3.0, -1.0);

    float2 f2XY = PosXY[VertexId];
    
    Pos = float4(f2XY, 1.0, 1.0);
    ClipPos = Pos;
}

#define NUM_SAMPLES 512u
//BRDF_LUT是通过二维uv来查询的，所以为了符合DX的uv规范(左上角(0,0)右下角(1,1)，对y进行颠倒)
float2 NormalizedDeviceXYToTexUV(float2 f2ProjSpaceXY)
{
    return float2(0.5, 0.5) + float2(0.5, -0.5) * f2ProjSpaceXY.xy;
}

float2 Hammersley2D(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N)
{
    float a = Roughness * Roughness;
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

void PS(in  float4 Pos     : SV_Position,
    in  float4 ClipPos : CLIP_POS,
    out float2 Color : SV_Target)
{
    float2 UV = NormalizedDeviceXYToTexUV(ClipPos.xy);
    float NoV = UV.x;
    float linearRoughness = UV.y;
    
    float3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos
    const float3 N = float3(0.0, 0.0, 1.0);
    float scale = 0.0;
    float bias = 0.0;
    for (uint i = 0u; i < NUM_SAMPLES; i++)
    {
        float2 Xi = Hammersley2D(i, NUM_SAMPLES);
        float3 H = ImportanceSampleGGX(Xi, linearRoughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));
        if (NoL > 0.0)
        {
            // https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
            // Also see eq. 92 in  https://google.github.io/filament/Filament.md.html#lighting/imagebasedlights
            // Note that VoH / (NormalDistribution_GGX(H,Roughness) * NoH) term comes from importance sampling
            
            //https://google.github.io/filament/Filament.md.html#importancesamplingfortheibl
            //为什么会多一个4，因为这里原本是采样入射光L，但不知道L的分布，只知道D项分布，所以采样的是H，由此引入了Jacobian of the transform
            float G_Vis = 4.0 * SmithGGXVisibilityCorrelated(NoL, NoV, linearRoughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            scale += (1.0 - Fc) * G_Vis;
            bias += Fc * G_Vis;
        }
    }
    Color = float2(scale, bias) / float(NUM_SAMPLES);
}