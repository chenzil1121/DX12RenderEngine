#ifndef TONEMAPPING_HLSL
#define TONEMAPPING_HLSL
struct ToneMappingAttribs
{                                
    // Middle gray value used by tone mapping operators.
    float fMiddleGray;
    // White point to use in tone mapping.
    float fWhitePoint;
    // Luminance point to use in tone mapping.
    float fLuminanceSaturation;
};

#ifndef RGB_TO_LUMINANCE
#   define RGB_TO_LUMINANCE float3(0.212671, 0.715160, 0.072169)
#endif

float3 Uncharted2Tonemap(float3 x)
{
    // http://www.gdcvault.com/play/1012459/Uncharted_2__HDR_Lighting
    // http://filmicgames.com/archives/75 - the coefficients are from here
    float A = 0.15; // Shoulder Strength
    float B = 0.50; // Linear Strength
    float C = 0.10; // Linear Angle
    float D = 0.20; // Toe Strength
    float E = 0.02; // Toe Numerator
    float F = 0.30; // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F; // E/F = Toe Angle
}

float3 ToneMap(in float3 f3Color, ToneMappingAttribs Attribs, float fAveLogLum)
{
    //const float middleGray = 1.03 - 2 / (2 + log10(fAveLogLum+1));
    float middleGray = Attribs.fMiddleGray;
    // Compute scale factor such that average luminance maps to middle gray
    float fLumScale = middleGray / fAveLogLum;

    f3Color = max(f3Color, float3(0.0, 0.0, 0.0));
    float fInitialPixelLum = max(dot(RGB_TO_LUMINANCE, f3Color), 1e-10);
    float fScaledPixelLum = fInitialPixelLum * fLumScale;
    float3 f3ScaledColor = f3Color * fLumScale;

    float whitePoint = Attribs.fWhitePoint;

    // TONE_MAPPING_MODE_UNCHARTED2
    // http://filmicgames.com/archives/75
    float ExposureBias = 2.0;
    float3 curr = Uncharted2Tonemap(ExposureBias * f3ScaledColor);
    float3 whiteScale = float3(1.0, 1.0, 1.0) / Uncharted2Tonemap(float3(whitePoint, whitePoint, whitePoint));
    return curr * whiteScale;
}
#endif