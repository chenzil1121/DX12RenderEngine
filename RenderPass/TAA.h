#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Sampler.h"
//暂时没有抖动

class TAA
{
public:
	TAA(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void CreateTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	void Compute(ComputeContext& Context, TextureViewer* GbufferSRV);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	size_t threadX;
	size_t threadY;
	size_t threadZ = 1;

	float alpha = 0.1;
	float colorBoxSigma = 1;

	std::unique_ptr<ComputePSO>taaPSO;
	std::unique_ptr<RootSignature>taaRS;

	std::unique_ptr<Texture> prevColor;
	std::unique_ptr<Texture> curColor;
	std::unique_ptr<Texture> output;

	
	std::unique_ptr<TextureViewer> colorSRV;
	std::unique_ptr<TextureViewer> outputUAV;

	unsigned int frameCount = 0;
};