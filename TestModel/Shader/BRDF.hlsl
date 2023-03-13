#ifndef BRDF_HLSL
#define BRDF_HLSL
#include "Common.hlsl"
struct BRDFData
{
	float3 baseColor;
	float3 DiffuseColor;
	float3 F0;
	float3 F90;
	float roughness;
	float metallic;
};

static const float F0_DIELECTRIC = 0.04f;

// [Burley 2012, "Physically-Based Shading at Disney"]
float3 Diffuse_Burley_Disney(float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH)
{
	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
	float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
	float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);
	return DiffuseColor * ((1 / PI) * FdV * FdL);
}

// Lambertian diffuse
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
float3 LambertianDiffuse(float3 DiffuseColor)
{
	return DiffuseColor / PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel term from "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
//
//      Rf(Theta) = Rf(0) + (1 - Rf(0)) * (1 - cos(Theta))^5
//
//
//           '.       |       .'
//             '.     |Theta.'
//               '.   |   .'
//                 '. | .'
//        ___________'.'___________
//                   '|
//                  ' |
//                 '  |
//                '   |
//               ' Phi|
//
// Note that precise relfectance term is given by the following expression:
//
//      Rf(Theta) = 0.5 * (sin^2(Theta - Phi) / sin^2(Theta + Phi) + tan^2(Theta - Phi) / tan^2(Theta + Phi))
//
#define SCHLICK_REFLECTION(VdotH, Reflectance0, Reflectance90) ((Reflectance0) + ((Reflectance90) - (Reflectance0)) * pow(clamp(1.0 - (VdotH), 0.0, 1.0), 5.0))
float3 SchlickReflection(float VdotH, float3 Reflectance0, float3 Reflectance90)
{
	return SCHLICK_REFLECTION(VdotH, Reflectance0, Reflectance90);
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float NormalDistribution_GGX(float NdotH, float AlphaRoughness)
{
	float a2 = AlphaRoughness * AlphaRoughness;
	float f = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / (PI * f * f);
}

// Visibility = G(v,l,a) / (4 * (n,v) * (n,l))
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float SmithGGXVisibilityCorrelated(float NdotL, float NdotV, float AlphaRoughness)
{
	float a2 = AlphaRoughness * AlphaRoughness;

	float GGXV = NdotL * sqrt(max(NdotV * NdotV * (1.0 - a2) + a2, 1e-7));
	float GGXL = NdotV * sqrt(max(NdotL * NdotL * (1.0 - a2) + a2, 1e-7));

	return 0.5 / (GGXV + GGXL);
}

float3 DirectLightBRDF(float3 L, float3 V, float3 N, inout BRDFData BRDF)
{
	float NoL = clamp(dot(N, L), 0.0, 1.0);
	float NoV = clamp(dot(N, V), 0.0, 1.0);
	float3 H = normalize(L + V);
	float NoH = clamp(dot(N, H), 0.0, 1.0);
	float LoH = clamp(dot(L, H), 0.0, 1.0);
	float VoH = clamp(dot(V, H), 0.0, 1.0);

	float3 baseColor_Linear = sRGB_Linear(BRDF.baseColor);
	BRDF.baseColor = baseColor_Linear;
	float3 DiffuseColor = (float3(1.0, 1.0, 1.0) - F0_DIELECTRIC.rrr) * (1.0 - BRDF.metallic) * baseColor_Linear;
	BRDF.DiffuseColor = DiffuseColor;

	float3 F0 = lerp(F0_DIELECTRIC.rrr, baseColor_Linear, BRDF.metallic);
	BRDF.F0 = F0;

	float reflectance = max(max(F0.r, F0.g), F0.b);
	// Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
	float3 F90 = clamp(reflectance * 50.0, 0.0, 1.0) * float3(1.0, 1.0, 1.0);
	BRDF.F90 = F90;

	float AlphaRoughness = BRDF.roughness * BRDF.roughness;

	if (NoL <= 0.0 && NoV <= 0.0)
		return float3(0.0, 0.0, 0.0);
	//float3 DiffuseBRDF = Diffuse_Burley_Disney(DiffuseColor, BRDFData.roughness, NoV, NoL, VoH);
	float3 DiffuseBRDF = LambertianDiffuse(DiffuseColor);
	
	float D = NormalDistribution_GGX(NoH, AlphaRoughness);
	float3 F = SchlickReflection(VoH, F0, F90);
	float G = SmithGGXVisibilityCorrelated(NoL, NoV, AlphaRoughness);
	// G项的分子和Cook-Torrance Specular BRDF的分母相消
	float3 SpecularBRDF = D * F * G;
	return  (1.0 - F) * DiffuseBRDF + SpecularBRDF;
}
#endif