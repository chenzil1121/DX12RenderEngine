#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"

class SVGFPass
{
public:
	SVGFPass(RenderDevice* pCore, SwapChain* swapChain, bool isDemodulate = false, int iterations = 3);

	void Create(bool isDemodulate);

	void CreateTexture();

	void ClearTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	TextureViewer* GetFilteredSRV()
	{
		if ((filterIterations) % 2 == 0)
		{
			return illuminationPongSRV.get();
		}
		else
		{
			return illuminationPingSRV.get();
		}
	}

	void Compute(ComputeContext& Context, TextureViewer* GbufferSRV, TextureViewer* PreNormalAndLinearZSRV, TextureViewer* RayTracingOutputSRV);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<ComputePSO>ReprojectPSO;
	std::unique_ptr<ComputePSO>EstimateVarPSO;
	std::unique_ptr<ComputePSO>AtrousPSO;

	std::unique_ptr<RootSignature>ReprojectRS;
	std::unique_ptr<RootSignature>EstimateVarRS;
	std::unique_ptr<RootSignature>AtrousRS;

	size_t threadX;
	size_t threadY;
	size_t threadZ = 1;

	float alpha = 0.05f;
	float momentsAlpha = 0.9f;
	float phiColor = 4.0;
	float phiNormal = 128.0; 
	int step = 0;

	int filterIterations = 3;


	std::unique_ptr<Texture> accIllumination;
	std::unique_ptr<Texture> accHistoryLength;
	std::unique_ptr<Texture> accMoments;
	std::unique_ptr<TextureViewer> accSRV;

	std::unique_ptr<Texture> illuminationPing;
	std::unique_ptr<Texture> illuminationPong;
	std::unique_ptr<Texture> historyLength;
	std::unique_ptr<Texture> moments;
	std::unique_ptr<TextureViewer> illuminationPingUAV;
	std::unique_ptr<TextureViewer> illuminationPingSRV;
	std::unique_ptr<TextureViewer> illuminationPongUAV;
	std::unique_ptr<TextureViewer> illuminationPongSRV;
	std::unique_ptr<TextureViewer> curUAV;
	std::unique_ptr<TextureViewer> curSRV;
};