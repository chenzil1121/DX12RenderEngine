#pragma once
#include"RenderDevice.h"
#include"SwapChain.h"
#include"Sampler.h"
#include"Buffer.h"

struct FXAAConstants
{
	float mrRcpTexDim[2];
	float mQualitySubPix = 0.75f;
	float mQualityEdgeThreshold = 0.166f;
	float mQualityEdgeThresholdMin = 0.0833f;
	bool mEarlyOut = true;
};

class FXAA
{
public:
	FXAA(RenderDevice* pCore, SwapChain* swapChain);
	void Create();

	void CreateTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	void Render(GraphicsContext& Context);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<GraphicsPSO>fxaaPSO;
	std::unique_ptr<RootSignature>fxaaRS;

	std::unique_ptr<Texture> inputColor;
	std::unique_ptr<TextureViewer> inputColorSRV;
	std::unique_ptr<Buffer> constantsBuffer;
};