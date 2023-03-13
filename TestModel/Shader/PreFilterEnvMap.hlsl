/*
* PreFilterEnvMap��PreIrradiance������Ҫ������
* PreIrradiance�����������Ͼ��Ȳ����Ļ�������Diffuse Light
* PreFilterEnvMap�ǰ�������N=VΪǰ���Specular�����Light��Ҫ�Բ����Ļ���
* Specular indirect Light�ĺ������ڸ�һ��V(����ӽ�����)����Ҫ֪����ӦSpecular BRDF����ļ���������ܺ�
* ������һ�����ƣ����������BRDF�ڰ�������״��һ���ģ�ƫ�뷨��45��۲��ƫ�뷨��0��(�ӷ��߽Ƕȹ۲�)��BRDF��״��һ���ģ�
* ��˿�����Ԥ����PreFilterEnvMap��ʱ�����ǿ��԰�N=V����ǰ�ᡣ��ô���ĺô��ǣ�ʵʱ��Ⱦ��ӹⲿ��ʱ������ֻ֪��N��V��
* ������N��V����reflect�������Եõ�R�����R�൱��Ԥ����ʱ���N����������Բ�ѯ����Ӧ��ӹ�Ļ��ֺ͡�
* Ԥ����ʱ���һ��Cube������������������N�൱�ڲ��������R������������R�����Բ鵽�Լ��ļ�ӹ�Ļ��ֺ͡�
*/

#include<Common.hlsl>

cbuffer cbTransform: register(b0)
{
    float4x4 g_Rotation;
    float g_Roughness;
    uint g_NumSamples;
    float2 Unuesd;
}

TextureCube  g_EnvMap : register(t0);
SamplerState g_samLinearClamp : register(s0);

void VS(in uint VertexId     : SV_VertexID,
    out float4 Pos : SV_Position,
    out float3 WorldPos : WORLD_POS)
{
    //����װ�䶥�㣬ʹ��D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP�γ�����������
    //�����������θ�����ȫ��
    float2 PosXY[4];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +1.0);
    PosXY[2] = float2(+1.0, -1.0);
    PosXY[3] = float2(+1.0, +1.0);
    Pos = float4(PosXY[VertexId], 1.0, 1.0);
    //Ĭ����+Z�棬ͨ��6����ͬ����ת�����γ�һ��ԭ��Ϊ���ĵİ�Χ�����壬��ԭ��(���λ��)��6������������ø�������������ķ��߲������
    //����IBL����������ÿһ����Ƭ��λ��CubeMap�γ����������
    float4 f4WorldPos = mul(Pos, g_Rotation);
    WorldPos = f4WorldPos.xyz / f4WorldPos.w;
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

void PS(in float4  Pos      : SV_Position,
    in float3  WorldPos : WORLD_POS,
    out float4 Color : SV_Target)
{
    float3 R = normalize(WorldPos);
    float3 N = R;
    float3 V = R;
    float3 PrefilteredColor = float3(0.0, 0.0, 0.0);

    float TotalWeight = 0.0;
    for (uint i = 0u; i < g_NumSamples; i++)
    {
        float2 Xi = Hammersley2D(i, g_NumSamples);
        float3 H = ImportanceSampleGGX(Xi, g_Roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        float NoL = clamp(dot(N, L), 0.0, 1.0);
        float VoH = clamp(dot(V, H), 0.0, 1.0);
        if (NoL > 0.0 && VoH > 0.0)
        {
            PrefilteredColor += g_EnvMap.Sample(g_samLinearClamp, L).rgb * NoL;
            TotalWeight += NoL;
        }
    }
    Color.rgb = PrefilteredColor / TotalWeight;
    Color.a = 1.0;
}

