//Root 0
Texture2D<float4>    g_PositionHit : register(t0);
Texture2D<float4>    g_AlbedoRoughness : register(t1);
Texture2D<float4>    g_NormalMetallic : register(t2);
Texture2D<float4>    g_MotionLinearZ : register(t3);
//Root 1
Texture2D<float4>    g_IlluminationIn : register(t4);
Texture2D<float>    g_HistoryLength : register(t5);
Texture2D<float2>    g_Moments : register(t6);
//Root 2
RWTexture2D<float4>    g_IlluminationOut : register(u0);
//Root 3
cbuffer RootConstants : register(b0)
{
    float g_PhiColor;
    float g_PhiNormal;
};

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float computeWeight(
    float depthCenter, float depthP, float phiDepth,
    float3 normalCenter, float3 normalP, float phiNormal,
    float luminanceIllumCenter, float luminanceIllumP, float phiIllum)
{
    const float weightNormal = pow(saturate(dot(normalCenter, normalP)), phiNormal);
    const float weightZ = (phiDepth == 0) ? 0.0f : abs(depthCenter - depthP) / phiDepth;
    const float weightLillum = abs(luminanceIllumCenter - luminanceIllumP) / phiIllum;

    const float weightIllum = exp(0.0 - max(weightLillum, 0.0) - max(weightZ, 0.0)) * weightNormal;

    return weightIllum;
}

[numthreads(8, 8, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 ipos = (int2)DispatchThreadID.xy;
    // no hit => must be envmap => do nothing
    if (g_PositionHit[ipos].w == 0)
    {
        g_IlluminationOut[ipos] = g_IlluminationIn[ipos];
        return;
    }

    float h = g_HistoryLength[ipos];

    int2 screenSize;
    g_HistoryLength.GetDimensions(screenSize.x, screenSize.y);

    if (h < 4.0) // not enough temporal history available
    {
        float sumWIllumination = 0.0;
        float3 sumIllumination = float3(0.0, 0.0, 0.0);
        float2 sumMoments = float2(0.0, 0.0);

        const float4 illuminationCenter = g_IlluminationIn[ipos];
#ifdef REFLECT
        const float lIlluminationCenter = luminance(illuminationCenter.xyz);
#else
        const float lIlluminationCenter = illuminationCenter.x + illuminationCenter.y * 0.25;
#endif

        const float zCenter = g_MotionLinearZ[ipos].z;
        const float3 nCenter = g_NormalMetallic[ipos].xyz;

        const float phiLIllumination = g_PhiColor;
        const float phiDepth = 1.0;

        // compute first and second moment spatially. This code also applies cross-bilateral
        // filtering on the input illumination.
        const int radius = 3;

        for (int yy = -radius; yy <= radius; yy++)
        {
            for (int xx = -radius; xx <= radius; xx++)
            {
                const int2 p = ipos + int2(xx, yy);
                const bool inside = all(p >= int2(0, 0)) && all(p < screenSize);
                const bool samePixel = (xx == 0 && yy == 0);
                const float kernel = 1.0;

                if (inside)
                {
                    const float3 illuminationP = g_IlluminationIn[p].rgb;
                    const float2 momentsP = g_Moments[p].xy;
#ifdef REFLECT
                    const float lIlluminationP = luminance(illuminationP);
#else
                    const float lIlluminationP = illuminationP.x + illuminationP.y * 0.25;
#endif
                    const float zP = g_MotionLinearZ[p].z;
                    const float3 nP = g_NormalMetallic[p].xyz;

                    const float w = computeWeight(
                        zCenter, zP, phiDepth * length(float2(xx, yy)),
                        nCenter, nP, g_PhiNormal,
                        lIlluminationCenter, lIlluminationP, phiLIllumination);

                    sumWIllumination += w;
                    sumIllumination += illuminationP * w;
                    sumMoments += momentsP * w;
                }
            }
        }

        // Clamp sum to >0 to avoid NaNs.
        sumWIllumination = max(sumWIllumination, 1e-6f);

        sumIllumination /= sumWIllumination;
        sumMoments /= sumWIllumination;

        // compute variance using the first and second moments
        float variance = sumMoments.g - sumMoments.r * sumMoments.r;

        // give the variance a boost for the first frames
        variance *= 4.0 / h;

        g_IlluminationOut[ipos] = float4(sumIllumination, variance.r);
    }
    else
    {
        g_IlluminationOut[ipos] = g_IlluminationIn[ipos];
    }
}