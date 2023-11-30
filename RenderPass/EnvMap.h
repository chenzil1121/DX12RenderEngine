#pragma once
#include"RenderDevice.h"
#include"SwapChain.h"
#include"Buffer.h"
#include"TextureLoader.h"
#include"Sampler.h"

class EnvMap
{
public:
	EnvMap(RenderDevice* pCore, SwapChain* pSwapChain, std::string EnvMapPath);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer);

	TextureViewer* GetEnvMapView()
	{
		return envMapView.get();
	};
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> envMapRS;
	std::unique_ptr<GraphicsPSO> envMapPSO;

	std::unique_ptr<Texture> envMap;
	std::unique_ptr<TextureViewer> envMapView;
};