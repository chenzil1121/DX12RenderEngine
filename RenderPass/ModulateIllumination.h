#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "Scene.h"

class ModulateIllumination
{
public:
	ModulateIllumination(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Light pointLight, TextureViewer* GbufferSRV, TextureViewer* FilteredShadowSRV, TextureViewer* FilteredReflectionSRV);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<GraphicsPSO> modulateIlluminationPSO;
	std::unique_ptr<RootSignature> modulateIlluminationRS;

	std::unique_ptr<Buffer> pointLightConstantBuffer;
};