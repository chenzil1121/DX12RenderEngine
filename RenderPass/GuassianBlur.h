#pragma once
#include "RenderDevice.h"
#include "TextureViewer.h"
#include "Buffer.h"
#include "RenderParameter.h"
class GuassianBlur
{
public:
	GuassianBlur(RenderDevice* pCore, float sigma = 2.5f);

	void Create();

	void Compute(ComputeContext& Context, Texture* tex, TextureViewer* texSRV);

	void CalcGaussWeights(float sigma);
private:
	RenderDevice* pCore;

	std::unique_ptr<ComputePSO>HorzBlurPSO;
	std::unique_ptr<ComputePSO>VertBlurPSO;
	std::unique_ptr<RootSignature>GuassianBlurRS;

	std::unique_ptr<Texture>blurPing;
	std::unique_ptr<Texture>blurPong;
	std::unique_ptr<TextureViewer>blurPingUAV;
	std::unique_ptr<TextureViewer>blurPingSRV;
	std::unique_ptr<TextureViewer>blurPongUAV;
	std::unique_ptr<TextureViewer>blurPongSRV;

	GuassianBlurConstants constants;
	std::unique_ptr<Buffer> CB;

	const int maxBlurRadius = 5;
};