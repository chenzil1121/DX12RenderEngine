#include"BRDF.hlsl"
struct IBLContribution
{
	float3 Diffuse;
	float3 Specular;
};

IBLContribution IBL(
	in float3                 n,
	in float3                 v,
	in float                  PrefilteredEnvMipLevels,
	in BRDFData				  BRDF,
	in Texture2D              BRDF_LUT,
	in TextureCube            IrradianceMap,
	in TextureCube            PrefilteredEnvMap,
	in SamplerState           samLinearClamp)
{
	float NoV = clamp(dot(n, v), 0.0, 1.0);
	float3 reflection = normalize(reflect(-v, n));
	//???
	float lod = clamp(BRDF.roughness * PrefilteredEnvMipLevels, 0.0, PrefilteredEnvMipLevels);

	float2 brdfSamplePoint = clamp(float2(NoV, BRDF.roughness), float2(0.0, 0.0), float2(1.0, 1.0));
	float2 brdf = BRDF_LUT.Sample(samLinearClamp, brdfSamplePoint).rg;

	float3 diffuseLight = IrradianceMap.Sample(samLinearClamp, n).rgb;
	float3 specularLight = PrefilteredEnvMap.SampleLevel(samLinearClamp, reflection, lod).rgb;

	IBLContribution IBLContrib;
	IBLContrib.Diffuse = diffuseLight * BRDF.diffuseColor;
	IBLContrib.Specular = specularLight * (BRDF.f0 * brdf.x + BRDF.f90 * brdf.y);
	//IBLContrib.Specular = specularLight;
	return IBLContrib;
}