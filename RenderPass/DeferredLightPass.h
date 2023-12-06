#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "TextureViewer.h"
#include "Buffer.h"
#include "Sampler.h"
#include "VarianceShadowMap.h"

class DeferredLightPass
{
public:
	DeferredLightPass(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, TextureViewer* GbufferSRV, TextureViewer* IBLView, VarianceShadowMap* vsm, Scene* scene);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> DeferredLightRS;
	std::unique_ptr<GraphicsPSO> DeferredLightPSO;
};