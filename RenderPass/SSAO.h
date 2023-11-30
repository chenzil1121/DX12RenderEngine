#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"

class SSAO
{
public:
	SSAO(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void CreateTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	Texture* GetAO()
	{
		return ambientOcclusionMap.get();
	}

	TextureViewer* GetSRV()
	{
		return SRV.get();
	}

	void Compute(ComputeContext& Context, Buffer* PassConstantBuffer, TextureViewer* GbufferSRV);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<Texture> ambientOcclusionMap;
	std::unique_ptr<TextureViewer> SRV;
	std::unique_ptr<TextureViewer> UAV;

	size_t threadX;
	size_t threadY;
	size_t threadZ = 1;

	std::unique_ptr<RootSignature> SSAOPassRS;
	std::unique_ptr<ComputePSO> SSAOPassPSO;
};