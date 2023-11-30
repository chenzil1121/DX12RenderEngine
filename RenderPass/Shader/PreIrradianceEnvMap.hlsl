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
    //不用装配顶点，使用D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP形成两个三角形
    //这两个三角形覆盖了全屏
    float2 PosXY[4];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +1.0);
    PosXY[2] = float2(+1.0, -1.0);
    PosXY[3] = float2(+1.0, +1.0);
    Pos = float4(PosXY[VertexId], 1.0, 1.0);
    //默认是+Z面，通过6个不同的旋转矩阵，形成一个原点为中心的包围立方体，从原点(相机位置)到6个面的向量正好覆盖了整体球体的法线采样情况
    //这里IBL假设物体上每一个面片都位于CubeMap形成球体的中心
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
            //以法线为基础的切线坐标系转换到世界坐标系
            float3 tempVec = cos(phi) * right + sin(phi) * up;
            float3 sampleDir = cos(theta) * N + sin(theta) * tempVec;
            //这里cos(theta)是渲染方程中的NoL，sin(theta)是立体角积分化简为dTheta和dPhi的产物
            color += g_EnvMap.Sample(g_samLinearClamp, sampleDir).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }
    //这里考虑了LambertianDiffuse中1/PI，1/PI和pdf化简变成了PI
    //https://zhuanlan.zhihu.com/p/66518450
    Color = float4(PI * color / sampleCount, 1.0);
}