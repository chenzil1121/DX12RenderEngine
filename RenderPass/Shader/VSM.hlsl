#ifndef VSM_HLSL
#define VSM_HLSL

#define SAMPLE_STEP 5
#define PCF_NUM_SAMPLES 100
#define NUM_RINGS 10
#define PI 3.141592653589793
#define PI2 6.283185307179586

float rand_2to1(float2 uv)
{
	// 0 - 1
	const float a = 12.9898, b = 78.233, c = 43758.5453;
	float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, PI);
	return frac(sin(sn) * c);
}

void poissonDiskSamples(float2 randomSeed, out float2 poissonDisk[PCF_NUM_SAMPLES])
{
	float ANGLE_STEP = PI2 * float(NUM_RINGS) / float(PCF_NUM_SAMPLES);
	float INV_NUM_SAMPLES = 1.0 / float(PCF_NUM_SAMPLES);

	float angle = rand_2to1(randomSeed) * PI2;
	float radius = INV_NUM_SAMPLES;
	float radiusStep = radius;

	for (int i = 0; i < PCF_NUM_SAMPLES; i++)
	{
		poissonDisk[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
		radius += radiusStep;
		angle += ANGLE_STEP;
	}
}

float CalculateShadow(
	Texture2D ShadowMap,
	float4x4 LightVP,
	float3 WorldPos,
	float ShadowNearZ,
	float ShadowFarZ
)
{
	float4 lightSpacePosH = mul(float4(WorldPos, 1.0), LightVP);
	float SceneDepth = lightSpacePosH.z / lightSpacePosH.w;
	float2 lightSpaceCrd = lightSpacePosH.xy / lightSpacePosH.w * float2(0.5, -0.5) + float2(0.5, 0.5);

	int2 imageDim;
	ShadowMap.GetDimensions(imageDim.x, imageDim.y);
	int2 ipos = lightSpaceCrd * imageDim;

	if (any(ipos < int2(0, 0)) || any(ipos > imageDim - int2(1, 1)))
		return 1.0f;

	int num = 0;
	float sumd = 0.0;
	float sumdq = 0.0;

	//for (int i = -SAMPLE_STEP; i <= SAMPLE_STEP; i++)
	//{
	//	for (int j = -SAMPLE_STEP; j <= SAMPLE_STEP; j++)
	//	{
	//		int2 samplePos = ipos + int2(i, j);
	//		if (any(samplePos < int2(0, 0)) || any(samplePos > imageDim - int2(1, 1)))
	//			continue;

	//		float2 sampleValue = ShadowMap.Load(int3(samplePos, 0));
	//		sumd += sampleValue.x;
	//		sumdq += sampleValue.y;
	//		num++;
	//	}
	//}

	float2 uv = ipos / float2(imageDim);
	float2 poissonDisk[PCF_NUM_SAMPLES];
	poissonDiskSamples(uv, poissonDisk);
	for (int i = 0; i < PCF_NUM_SAMPLES; i++)
	{
		int2 samplePos = ipos + poissonDisk[i] * 5;
		if (any(samplePos < int2(0, 0)) || any(samplePos > imageDim - int2(1, 1)))
			continue;
		
		float2 sampleValue = ShadowMap.Load(int3(samplePos, 0));
		sumd += sampleValue.x;
		sumdq += sampleValue.y;
		num++;
	}

	if (num > 0)
	{
		sumd = sumd / num;
		sumdq = sumdq / num;

		float miu = sumd;
		float sigma2 = sumdq - miu * miu;
		float zDiff = SceneDepth - miu;
				
		float slf = zDiff <= 0 ? 1 : 0;
		float vsf = saturate(sigma2 / (sigma2 + zDiff * zDiff));

		return max(slf, vsf);
	}
	else
		return 1.0;
	
}

#endif