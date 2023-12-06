#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    int type;
};

struct RayPayload
{
    float4 color;
};
struct ShadowRayPayload
{
    bool hit;
};

struct RayTracingConstantBuffer
{
    float4 CameraPosition;
    uint FrameCount;
    uint VertexOffsetInHeap;
    uint IndexOffsetInHeap;
    uint PBRTexSRVOffsetInHeap;
    Light PointLight;
};

struct InstanceInfo
{
    int VertexBufferID;
    int IndexBufferID;
    int BaseColorTexID;
    int RoughnessMetallicTexID;
    int NormalTexID;
    int ipad0;
    int ipad1;
    int ipad2;
    float4 BaseColorFactor;
    float RoughnessFactor;
    float MetallicFactor;
    bool HasUV;
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float2 TexCoords;
};

//Root 0
RWTexture2D<float4> g_ShadowOutput : register(u0);
//Root 1
RWTexture2D<float4> g_ReflectOutput : register(u1);
//Root 2
RaytracingAccelerationStructure g_Scene : register(t0, space0);
//Root 3
StructuredBuffer<InstanceInfo> g_InstanceInfos : register(t1, space0);
//Root 4
ConstantBuffer<RayTracingConstantBuffer> g_RayTracingCB : register(b0);
//Root 5
TextureCube<float4> g_SkyboxTexture : register(t0, space1);
//Root 6
Texture2D<float4> g_PositionHit : register(t0, space3);
Texture2D<float4> g_AlbedoRoughness : register(t1, space3);
Texture2D<float4> g_NormalMetallic : register(t2, space3);
//Root 7
ByteAddressBuffer g_Indices[] : register(t3, space0);
StructuredBuffer<Vertex> g_Vertices[] : register(t3, space1);
Texture2D<float4> g_PBRTextures[] : register(t3, space2);


SamplerState g_samAnisotropicClamp : register(s0);
SamplerState g_samLinearWarp : register(s1);

//根据instanceId查询三角形顶点索引
uint3 Load3x32BitIndices(uint offsetBytes, uint instanceId)
{
    const uint3 indices = g_Indices[g_RayTracingCB.IndexOffsetInHeap + instanceId].Load3(offsetBytes);
    return indices;
}

float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

float3 HitAttributeFloat3(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float2 HitAttributeFloat2(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float3 sRGB_Linear(float3 x)
{
    return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

uint RNG(uint seed)
{
    // Condensed version of pcg_output_rxs_m_xs_32_32
    seed = seed * 747796405 + 1;
    seed = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737;
    seed = (seed >> 22) ^ seed;

    return seed;
}
float2 RNG(uint i, uint num)
{
    return float2(i / float(num), (RNG(i) & 0xffff) / float(0x10000));
}
float2 getSampleParam(uint2 index, uint2 dim, uint numSamples = 256)
{
    uint s = index.y * dim.x + index.x;

    s = RNG(s);
    s += g_RayTracingCB.FrameCount;
    s = RNG(s);
    s %= numSamples;

    return RNG(s, numSamples);
}

float3 SampleTan2W(float3 H, float3 N)
{
    float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec =  tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

//半球面均匀采样
float3 UniformSample(float2 Xi, float3 N)
{
    float PI = 3.141592653;

    float phi = 2.0 * PI * Xi.x;
    float theta = 0.5 * PI * Xi.y;

    //从球坐标到笛卡尔坐标
    float3 R;
    R.x = cos(phi) * sin(theta);
    R.y = sin(phi) * sin(theta);
    R.z = cos(theta);

    return normalize(SampleTan2W(R, N));
}

//圆锥体采样：在方向锥上均匀地采样射线
float3 UniformSampleCone(float2 u, float cosThetaMax)
{
    float cosTheta = (1.0f - u.x) + u.x * cosThetaMax;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    float phi = u.y * 2 * 3.141592653f;

    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Brian Karis, Epic Games "Real Shading in Unreal Engine 4"
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N)
{
    float PI = 3.14159265359;

    float m = Roughness * Roughness;
    float m2 = m * m;

    float Phi = 2 * PI * Xi.x;

    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (m2 - 1.0) * Xi.y));
    float SinTheta = sqrt(max(1e-5, 1.0 - CosTheta * CosTheta));

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    //return float3(Phi, Phi, Phi);
    return SampleTan2W(H, N);
}

//////////////////////////////////////////////////// PBR rendering method
// [Smith 1967, "Geometrical shadowing of a random rough surface"]
float Vis_Smith(float roughness, float NoV, float NoL)
{
    const float a = roughness * roughness;
    const float a2 = a * a;

    const float vis_SmithV = NoV + sqrt(NoV * (NoV - NoV * a2) + a2);
    const float vis_SmithL = NoL + sqrt(NoL * (NoL - NoL * a2) + a2);

    return 1.0 / (vis_SmithV * vis_SmithL);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    float fc = pow(1.0 - cosTheta, 5.0);
    // Anything less than 2% is physically impossible and is instead considered to be shadowing
    return saturate(50.0 * F0.g) * fc + (1.0 - fc) * F0;
}

float3 DoPbrRadiance(float3 radiance, float3 L, float3 N, float3 V, float3 P, float3 albedo, float roughness, float metallic)
{
    float PI = 3.14159265359;
    const float3 F0 = lerp(0.04, albedo, metallic);
    float3 H = normalize(V + L);

    // scale light by NdotL
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));

    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float vis = Vis_Smith(roughness, NdotV, NdotL);

    float3 F = fresnelSchlick(VdotH, F0);

    return NdotL * F * vis * (4.0 * VdotH / NdotH) * radiance;
}

float3 DoPbrDiffuse(float3 bounceColor, float3 albedo)
{
    return bounceColor * albedo * (1.0 - 0.04);
}

// Compute local direction first and transform it to world space
float3 computeLocalDirectionCos(float2 xi)
{
    float PI = 3.14159265359;

    const float phi = 2.0 * PI * xi.x;

    // Only near the specular direction according to the roughness for importance sampling
    const float cosTheta = sqrt(xi.y);
    //const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    const float sinTheta = sqrt(1.0 - xi.y);

    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 computeDirectionCos(float3 normal, float2 xi)
{
    const float3 localDir = computeLocalDirectionCos(xi);

    return normalize(normal + localDir);
}

float3 getFromNormalMapping(
    float3 sampledNormal,
    float3 NormalWS, 
    float3 vertexPosition[3], 
    float2 vertexUV[3], 
    float3x3 R,
    bool HasUV
)
{
    float3 normal = NormalWS;
    if (HasUV)
    {
        float3 tangentNormal = sampledNormal.xyz * 2.0 - 1.0; // [-1, 1]

        float3 deltaXYZ_1 = mul(R, vertexPosition[1]) - mul(R, vertexPosition[0]);
        float3 deltaXYZ_2 = mul(R, vertexPosition[2]) - mul(R, vertexPosition[0]);
        float2 deltaUV_1 = vertexUV[1] - vertexUV[0];
        float2 deltaUV_2 = vertexUV[2] - vertexUV[0];

        float3 N = normal;
        float3 T = normalize(deltaUV_2.y * deltaXYZ_1 - deltaUV_1.y * deltaXYZ_2);
        float3 B = normalize(cross(N, T));
        float3x3 TBN = float3x3(T, B, N);
        normal = normalize(mul(tangentNormal, TBN));
    }
    
    return normal;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    //屏幕当前像素宽度、高度、深度
    uint3 launchIndex = DispatchRaysIndex();
    //屏幕宽度、高度、深度
    uint3 launchDimension = DispatchRaysDimensions();

    //随机数
    const float2 xi = getSampleParam(launchIndex.xy, launchDimension.xy);

    float hit = g_PositionHit.Load(int3(launchIndex.x, launchIndex.y, 0)).w;
    if (hit <= 0)
    {
        g_ShadowOutput[launchIndex.xy] = float4(0, 0, 0, 1);
        g_ReflectOutput[launchIndex.xy] = float4(0, 0, 0, 1);
        return;
    }

    float3 position = g_PositionHit.Load(int3(launchIndex.x, launchIndex.y, 0)).xyz;
    float3 albedo = g_AlbedoRoughness.Load(int3(launchIndex.x, launchIndex.y, 0)).xyz;
    float roughness = g_AlbedoRoughness.Load(int3(launchIndex.x, launchIndex.y, 0)).w;
    float metallic = g_NormalMetallic.Load(int3(launchIndex.x, launchIndex.y, 0)).w;
    float3 normal = g_NormalMetallic.Load(int3(launchIndex.x, launchIndex.y, 0)).xyz;

    // Lighting is performed in world space.
    float3 P = position;
    float3 V = normalize(g_RayTracingCB.CameraPosition.xyz - position);
    float3 N = normal;
    // Make sure our normal is pointed the right direction
    if (dot(N, V) <= 0.0f) N = -N;

    //Shadow
    float Lo = 0;
    //AO
    float La = 0;
    //Reflect
    float3 Lr = 0;

    //RayTracing Shadow
    {
        RayDesc ray;
        ray.TMin = 0.0f;
        ShadowRayPayload shadowPayload;
        float bias = 1e-2f;
        float3 P_biased = P + N * bias;

        float3 dest = g_RayTracingCB.PointLight.Position.xyz;
        float distan = distance(P_biased, dest);
        float3 lightDir = (dest - P_biased) / distan;

        //点光源假设半径
        float sampleDestRadius = g_RayTracingCB.PointLight.FalloffEnd;

        float maxCosTheta = distan / sqrt(sampleDestRadius * sampleDestRadius + distan * distan);

        float3 distributedSampleAngleinTangentSpace = UniformSampleCone(xi, maxCosTheta);
        float3 distributedDir = normalize(SampleTan2W(distributedSampleAngleinTangentSpace, lightDir));
        // ray setup
        ray.Origin = P_biased;
        ray.Direction = distributedDir;
        ray.TMax = distan * length(distributedSampleAngleinTangentSpace) / distributedSampleAngleinTangentSpace.z;
        shadowPayload.hit = false;
        // ray tracing
        TraceRay(g_Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF, 0, 0, 0, ray, shadowPayload); // 4-命中组索引偏移量 6-miss shader索引
        if (shadowPayload.hit == false)
        {
            Lo = 1.0f;
        }
    }

    //RayTracing AO
    { 
        float minAOLength = 0.01f;
        float maxAOLength = 1.0f;
        RayDesc aoRay;
        ShadowRayPayload aoRayPayload;
        aoRay.Origin = P;
        aoRay.TMin = minAOLength;
        aoRay.TMax = maxAOLength;
        float3 aoDir = UniformSample(xi, N);
        aoRay.Direction = aoDir;
        TraceRay(g_Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF, 0, 0, 0, aoRay, aoRayPayload);
        if (aoRayPayload.hit == false)
        {
            La = 1.0f;
        }
    }

    //RayTracing Reflection
    {
        float3 H = ImportanceSampleGGX(xi, roughness, N);
        float3 L = reflect(-V, H); 

        RayDesc raySecondary;
        raySecondary.TMin = 0.0f;
        raySecondary.Origin = P;
        raySecondary.Direction = L;
        raySecondary.TMax = 10000.0f;
        RayPayload secondaryPayload;
        secondaryPayload.color = float4(0, 0, 0, 0);

        if (dot(L, N) >= 0) // trace reflected ray only if the ray is not below the surface
        {
            TraceRay(g_Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                0xFF, 1, 0, 1, raySecondary, secondaryPayload);
        }
        //base material specular
        if (secondaryPayload.color.w > 0.1f)
        {
            Lr += DoPbrRadiance(secondaryPayload.color.xyz, L, N, V, P, albedo, roughness, metallic);
        }
        //base material diffuse
        if (metallic < 1.0f)
        {
            RayDesc raySecondarydif;
            raySecondarydif.TMin = 0.0f;
            raySecondarydif.Origin = P;
            raySecondarydif.Direction = computeDirectionCos(N, xi);
            raySecondarydif.TMax = 10000.0f;
            RayPayload secondarydifPayload;
            secondarydifPayload.color = float4(0, 0, 0, 0);

            TraceRay(g_Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                0xFF, 1, 0, 1, raySecondarydif, secondarydifPayload);

            Lr += DoPbrDiffuse(secondarydifPayload.color.xyz, albedo);
        }
    }
  
    // Write the raytraced color to the output texture.
    g_ReflectOutput[DispatchRaysIndex().xy] = float4(Lr, 1.0);
    g_ShadowOutput[DispatchRaysIndex().xy] = float4(Lo, La, 0.0, 1.0);
}

//************************************************** for shadow chs***************************************************//
[shader("closesthit")]
void MyShadowClosestHitShader(inout ShadowRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hit = true;
}

[shader("miss")]
void MyShadowMissShader(inout ShadowRayPayload payload)
{
    payload.hit = false;
}

//************************************************** for reflection chs**************************************************//
[shader("closesthit")]
void MySecondaryClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 hitPosition = HitWorldPosition();

    uint instanceId = InstanceID();
    InstanceInfo instanceInfo = g_InstanceInfos[instanceId];
    //屏幕当前像素宽度、高度、深度
    uint3 launchIndex = DispatchRaysIndex(); 
    //屏幕宽度、高度、深度
    uint3 launchDimension = DispatchRaysDimensions(); 
    float2 xi = getSampleParam(launchIndex.xy, launchDimension.xy);

    // Get the base index of the triangle's first 32 bit index.
    uint indexSizeInBytes = 4;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up 3 32 bit indices for the triangle.
    const uint3 indices = Load3x32BitIndices(baseIndex, instanceInfo.IndexBufferID);

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexPositions[3] =
    {
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[0]].Position,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[1]].Position,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[2]].Position
    };

    float3 vertexNormals[3] = { 
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[0]].Normal,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[1]].Normal,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[2]].Normal
    };

    float2 vertexTexCoord[3] =
    {
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[0]].TexCoords,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[1]].TexCoords,
        g_Vertices[g_RayTracingCB.VertexOffsetInHeap + instanceInfo.VertexBufferID][indices[2]].TexCoords
    };

    // Compute the triangle's normal.
    float3 Normal = HitAttributeFloat3(vertexNormals, attr);
    float2 TexCoord = HitAttributeFloat2(vertexTexCoord, attr);
    //WorldToObject3x4 是 ObjectToWorld4x3 的逆转置
    float3 worldNormal = normalize(mul((Normal), (float3x3)WorldToObject3x4()));

    //material parameter
    float3 albedo = (g_PBRTextures[g_RayTracingCB.PBRTexSRVOffsetInHeap + instanceInfo.BaseColorTexID].SampleLevel(g_samLinearWarp, TexCoord, 0) * instanceInfo.BaseColorFactor).rgb;
    albedo = sRGB_Linear(albedo);
    float metallic = g_PBRTextures[g_RayTracingCB.PBRTexSRVOffsetInHeap + instanceInfo.RoughnessMetallicTexID].SampleLevel(g_samLinearWarp, TexCoord, 0).b * instanceInfo.MetallicFactor;
    float roughness = g_PBRTextures[g_RayTracingCB.PBRTexSRVOffsetInHeap + instanceInfo.RoughnessMetallicTexID].SampleLevel(g_samLinearWarp, TexCoord, 0).g * instanceInfo.RoughnessFactor;

    float3 P = hitPosition;
    //float3 N = getFromNormalMapping(sampleNormal, worldNormal, vertexPositions, vertexTexCoord, (float3x3)ObjectToWorld3x4(), instanceInfo.HasUV);
    float3 N = worldNormal;
    float3 V = -WorldRayDirection();

    float3 color = 0.0f;
    //base material diffuse
    if (metallic < 0.5f)
    {
        float3 direction = computeDirectionCos(N, xi);
        float3 bounceColor = g_SkyboxTexture.SampleLevel(g_samAnisotropicClamp, direction, 0).xyz;
        color += DoPbrDiffuse(bounceColor, albedo);
    }

    //base material specular
    float3 H = ImportanceSampleGGX(xi, roughness, N);
    float3 direction = reflect(-V, H);
    float3 bounceColor = g_SkyboxTexture.SampleLevel(g_samAnisotropicClamp, direction, 0).xyz;

    color += DoPbrRadiance(bounceColor, direction, N, V, P, albedo, roughness, metallic);

    payload.color = float4(color, 1.0f);
}

[shader("miss")]
void MySecondaryMissShader(inout RayPayload payload)
{
    float3 direction = normalize(WorldRayDirection());
    float3 skyboxsample = g_SkyboxTexture.SampleLevel(g_samAnisotropicClamp, direction, 0).xyz;
    payload.color = float4(skyboxsample, 1.0f);
}
#endif // RAYTRACING_HLSL