//Root 0
Texture2D<float4>    g_PositionHit : register(t0);
Texture2D<float4>    g_AlbedoRoughness : register(t1);
Texture2D<float4>    g_NormalMetallic : register(t2);
Texture2D<float4>    g_MotionLinearZ : register(t3);
//Root 1
cbuffer RootConstants : register(b0)
{
    float g_Step;
    float g_PhiColor;
    float g_PhiNormal;
};
//Root 2
Texture2D<float4>    g_IlluminationIn : register(t4);
//Root 3
RWTexture2D<float4>    g_IlluminationOut : register(u0);


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

// computes a 3x3 gaussian blur of the variance, centered around
// the current pixel
float computeVarianceCenter(int2 ipos)
{
    float sum = 0.f;

    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    const int radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            const int2 p = ipos + int2(xx, yy);
            const float k = kernel[abs(xx)][abs(yy)];
            sum += g_IlluminationIn.Load(int3(p, 0)).a * k;
        }
    }

    return sum;
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

    int2 screenSize;
    g_IlluminationIn.GetDimensions(screenSize.x, screenSize.y);

    const float epsVariance = 1e-10;
    const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    // constant samplers to prevent the compiler from generating code which
    // fetches the sampler descriptor from memory for each texture access
    const float4 illuminationCenter = g_IlluminationIn.Load(int3(ipos, 0));
#ifdef REFLECT
    const float lIlluminationCenter = luminance(illuminationCenter.xyz);
#else
    const float lIlluminationCenter = illuminationCenter.x + illuminationCenter.y * 0.25;
#endif

    // variance, filtered using 3x3 gaussin blur
    const float var = computeVarianceCenter(ipos);

    const float zCenter = g_MotionLinearZ[ipos].z;
    const float3 nCenter = g_NormalMetallic[ipos].xyz;

    const float phiLIllumination = g_PhiColor * sqrt(max(0.0, epsVariance + var));
    const float phiDepth = g_Step;

    // explicitly store/accumulate center pixel with weight 1 to prevent issues
    // with the edge-stopping functions
    float sumWIllumination = 1.0;
    float4  sumIllumination = illuminationCenter;
    for (int yy = -2; yy <= 2; yy++)
    {
        for (int xx = -2; xx <= 2; xx++)
        {
            const int2 p = ipos + int2(xx, yy) * int(g_Step);
            const bool inside = all(p >= int2(0, 0)) && all(p < screenSize);

            const float kernel = kernelWeights[abs(xx)] * kernelWeights[abs(yy)];

            if (inside && (xx != 0 || yy != 0)) // skip center pixel, it is already accumulated
            {
                const float4 illuminationP = g_IlluminationIn.Load(int3(p, 0));
#ifdef REFLECT
                const float lIlluminationP = luminance(illuminationP.xyz);
#else
                const float lIlluminationP = illuminationP.x + illuminationP.y * 0.25;
#endif

                const float zP = g_MotionLinearZ[p].z;
                const float3 nP = g_NormalMetallic[p].xyz;

                // compute the edge-stopping functions
                const float w = computeWeight(
                    zCenter, zP, phiDepth * length(float2(xx, yy)),
                    nCenter, nP, g_PhiNormal,
                    lIlluminationCenter, lIlluminationP, phiLIllumination);

                const float wIllumination = w * kernel;

                // alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula
                sumWIllumination += wIllumination;
                sumIllumination += float4(wIllumination.xxx, wIllumination * wIllumination) * illuminationP;
            }
        }
    }

    // renormalization is different for variance, check paper for the formula
    float4 filteredIllumination = float4(sumIllumination / float4(sumWIllumination.xxx, sumWIllumination * sumWIllumination));

    g_IlluminationOut[ipos] = filteredIllumination;
}