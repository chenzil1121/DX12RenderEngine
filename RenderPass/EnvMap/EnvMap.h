#pragma once
#include"RenderDevice.h"
#include"SwapChain.h"
#include"Buffer.h"
#include"TextureLoader.h"
#include"Sampler.h"
class EnvMap
{
public:
	EnvMap(RenderDevice* pDevice, SwapChain* pSwapChain, std::string EnvMapPath);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer);

	TextureViewer* GetEnvMapView() { return m_EnvMapView.get(); }
private:
	RenderDevice* m_pDevice;
	SwapChain* m_pSwapChain;

	std::unique_ptr<RootSignature> m_EnvMapRS;
	std::unique_ptr<GraphicsPSO> m_EnvMapPSO;

	std::unique_ptr<Texture> m_EnvMap;
	std::unique_ptr<TextureViewer>m_EnvMapView;
};