#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"

class BMFRPass
{
public:
	BMFRPass(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void CreateTexture();

	void ClearTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	TextureViewer* GetFilteredReflectionSRV()
	{
		return curNoisySRV.get();
	}

	void Compute(ComputeContext& Context, TextureViewer* GbufferSRV, TextureViewer* PreNormalAndLinearZSRV, TextureViewer* RayTracingOutputSRV);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<ComputePSO>PreprocessPSO;
	std::unique_ptr<ComputePSO>RegressionPSO;
	std::unique_ptr<ComputePSO>PostprocessPSO;
	std::unique_ptr<GraphicsPSO>ReflectionTestPSO;

	std::unique_ptr<RootSignature>PreprocessRS;
	std::unique_ptr<RootSignature>RegressionRS;
	std::unique_ptr<RootSignature>PostprocessRS;
	std::unique_ptr<RootSignature>ReflectionTestRS;

	size_t threadX;
	size_t threadY;
	size_t threadZ = 1;
	size_t blockSize = 32;
	uint32_t frameCount = 0;

	std::unique_ptr<Texture> prevNoisy;
	std::unique_ptr<Texture> curNoisy;
	std::unique_ptr<Texture> acceptBools;
	std::unique_ptr<Texture> prevFramePixel;
	std::unique_ptr<Texture> tmpData[8];
	std::unique_ptr<Texture> outData[8];

	std::unique_ptr<TextureViewer> preProcessTexUAV;
	std::unique_ptr<TextureViewer> tmpDataUAV;
	std::unique_ptr<TextureViewer> outDataUAV;
	std::unique_ptr<TextureViewer> curNoisyUAV;
	std::unique_ptr<TextureViewer> curNoisySRV;
	std::unique_ptr<TextureViewer> postProcessTexSRV;
};