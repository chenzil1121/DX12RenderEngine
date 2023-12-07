//Root 0
Texture2D<float4>    g_PositionHit : register(t0);
Texture2D<float4>    g_AlbedoRoughness : register(t1);
Texture2D<float4>    g_NormalMetallic : register(t2);
Texture2D<float4>    g_MotionLinearZ : register(t3);
//Root 1
Texture2D<float4>    g_PreNormalMetallic : register(t4);
Texture2D<float4>    g_PreMotionLinearZ : register(t5);
//Root 2
Texture2D<float4>    g_AccIllumination : register(t6);
Texture2D<float>    g_AccHistoryLength : register(t7);
Texture2D<float2>    g_AccMoments : register(t8);
//Root 3
Texture2D<float4>    g_Input : register(t9);
//Root 4
RWTexture2D<float4>    g_Illumination : register(u0);
RWTexture2D<float>    g_HistoryLength : register(u1);
RWTexture2D<float2>    g_Moments : register(u2);
//Root 5
cbuffer RootConstants : register(b0)
{
    float g_Alpha;
    float g_MomentsAlpha;
};

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 demodulate(float3 c, float3 albedo)
{
    return c / max(albedo, float3(0.001, 0.001, 0.001));
}

bool isReprjValid(int2 coord, float Z, float Zprev, float3 normal, float3 normalPrev)
{
    int2 imageDim;
    g_Input.GetDimensions(imageDim.x, imageDim.y);

    // check whether reprojected pixel is inside of the screen
    if (any(coord < int2(1, 1)) || any(coord > imageDim - int2(1, 1))) return false;

    // check if deviation of depths is acceptable
    if (abs(Zprev - Z) / 1e-2f > 10.f) return false;

    // check normals for compatibility
    if (distance(normal, normalPrev) / 1e-2 > 16.0) return false;

    return true;
}

bool loadPrevData(int2 ipos, out float4 prevIllum, out float2 prevMoments, out float historyLength)
{
    int2 imageDim;
    g_Input.GetDimensions(imageDim.x, imageDim.y);
    const float2 motion = g_MotionLinearZ[ipos].xy;
    // +0.5 to account for texel center offset
    const int2 iposPrev = int2(float2(ipos)+motion * imageDim + float2(0.5, 0.5));

    float depth = g_MotionLinearZ[ipos].z;
    float3 normal = g_NormalMetallic[ipos].xyz;

    prevIllum = float4(0.0, 0.0, 0.0, 0.0);
    prevMoments = float2(0.0, 0.0);

    bool v[4];
    const float2 posPrev = float2(ipos)+motion * imageDim;
    const int2 offset[4] = { int2(0,0),int2(1,0), int2(0,1), int2(1,1) };

    // check for all 4 taps of the bilinear filter for validity
    bool valid = false;
    for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
    {
        int2 loc = int2(posPrev)+offset[sampleIdx];
        float depthPrev = g_PreMotionLinearZ[loc].z;
        float3 normalPrev = g_PreNormalMetallic[loc].xyz;

        v[sampleIdx] = isReprjValid(iposPrev, depth, depthPrev, normal, normalPrev);

        valid = valid || v[sampleIdx];
    }

    if (valid)
    {
        float sumw = 0;
        float x = frac(posPrev.x);
        float y = frac(posPrev.y);

        // bilinear weights
        const float w[4] = 
        { 
            (1 - x) * (1 - y), 
                 x  * (1 - y),
            (1 - x) *  y,
                 x  *  y 
        };

        // perform the actual bilinear interpolation
        for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
        {
            const int2 loc = int2(posPrev)+offset[sampleIdx];
            if (v[sampleIdx])
            {
                prevIllum += w[sampleIdx] * g_AccIllumination[loc];
                prevMoments += w[sampleIdx] * g_AccMoments[loc];
                sumw += w[sampleIdx];
            }
        }
        // redistribute weights in case not all taps were used
        valid = (sumw >= 0.01);
        prevIllum = valid ? prevIllum / sumw : float4(0, 0, 0, 0);
        prevMoments = valid ? prevMoments / sumw : float2(0, 0);
    }

    if (!valid) // perform 3*3 filter in the hope to find some suitable samples somewhere
    {
        float nValid = 0.0;

        const int radius = 1;
        for (int yy = -radius; yy <= radius; yy++)
        {
            for (int xx = -radius; xx <= radius; xx++)
            {
                const int2 p = iposPrev + int2(xx, yy);
                const float depthFilter = g_PreMotionLinearZ[p].z;
                const float3 normalFilter = g_PreNormalMetallic[p].xyz;

                if (isReprjValid(iposPrev, depth, depthFilter, normal, normalFilter))
                {
                    prevIllum += g_AccIllumination[p];
                    prevMoments += g_AccMoments[p];
                    nValid += 1.0;
                }
            }
        }
        if (nValid > 0)
        {
            valid = true;
            prevIllum /= nValid;
            prevMoments /= nValid;
        }
    }

    if (valid)
    {
        historyLength = g_AccHistoryLength[iposPrev];
    }
    else
    {
        prevIllum = float4(0, 0, 0, 0);
        prevMoments = float2(0, 0);
        historyLength = 0;
    }

    return valid;
}

[numthreads(8, 8, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
	int2 ipos = (int2)DispatchThreadID.xy;

#ifdef REFLECT
    float3 illumination = demodulate(g_Input[ipos].rgb, g_AlbedoRoughness[ipos].rgb);
    if (isnan(illumination.x) || isnan(illumination.y) || isnan(illumination.z))
        illumination = float3(0, 0, 0);
#else
    float3 illumination = g_Input[ipos].xyz;
#endif

    float historyLength;
    float4 prevIllumination;
    float2 prevMoments;
    bool success = loadPrevData(ipos, prevIllumination, prevMoments, historyLength);
    historyLength = min(32.0f, success ? historyLength + 1.0f : 1.0f);

    // this adjusts the alpha for the case where insufficient history is available.
    // It boosts the temporal accumulation to give the samples equal weights in
    // the beginning.
    const float alpha = success ? max(g_Alpha, 1.0 / historyLength) : 1.0;
    const float alphaMoments = success ? max(g_MomentsAlpha, 1.0 / historyLength) : 1.0;

    float2 moments;
#ifdef REFLECT
    moments.r = luminance(illumination);
#else
    moments.r = illumination.x + illumination.y * 0.25;
#endif
    moments.g = moments.r * moments.r;

    // temporal integration of the moments and illumination
    moments = lerp(prevMoments, moments, alphaMoments);

    float variance = max(0.f, moments.g - moments.r * moments.r);

    g_Illumination[ipos] = float4(lerp(prevIllumination.rgb, illumination, alpha), variance);
    g_HistoryLength[ipos] = historyLength;
    g_Moments[ipos] = moments;
}