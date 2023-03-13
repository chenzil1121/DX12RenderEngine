#ifndef NUM_PHI_SAMPLES
#   define NUM_PHI_SAMPLES 64
#endif

#ifndef NUM_THETA_SAMPLES
#   define NUM_THETA_SAMPLES 32
#endif

cbuffer cbTransform: register(b0)
{
    float4x4 g_Rotation;
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

void PS(in float4 Pos      : SV_Position,
    in float3 WorldPos : WORLD_POS,
    out float4 Color : SV_Target)
{
    float3 N = normalize(WorldPos);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    const float PI = 3.14159265;
    const float deltaPhi = 2.0 * PI / float(NUM_PHI_SAMPLES);
    const float deltaTheta = 0.5 * PI / float(NUM_THETA_SAMPLES);

    float3 color = float3(0.0, 0.0, 0.0);
    float sampleCount = 0.0;
    for (int p = 0; p < NUM_PHI_SAMPLES; ++p)
    {
        float phi = float(p) * deltaPhi;
        for (int t = 0; t < NUM_THETA_SAMPLES; ++t)
        {
            float theta = float(t) * deltaTheta;
            //�Է���Ϊ��������������ϵת������������ϵ
            float3 tempVec = cos(phi) * right + sin(phi) * up;
            float3 sampleDir = cos(theta) * N + sin(theta) * tempVec;
            //����cos(theta)����Ⱦ�����е�NoL��sin(theta)������ǻ��ֻ���ΪdTheta��dPhi�Ĳ���
            color += g_EnvMap.Sample(g_samLinearClamp, sampleDir).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }
    //���￼����LambertianDiffuse��1/PI��1/PI��pdf��������PI
    //https://zhuanlan.zhihu.com/p/66518450
    Color = float4(PI * color / sampleCount, 1.0);
}