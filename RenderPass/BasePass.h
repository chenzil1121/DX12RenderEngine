#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "IBL.h"
#include "Scene.h"

class BasePass
{
public:
	BasePass(RenderDevice* pCore, SwapChain* swapchain);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Scene* model, IBL* ibl);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> BasePassRS;
	std::unique_ptr<GraphicsPSO> BasePassOpaquePSO;
	std::unique_ptr<GraphicsPSO> BasePassBlendPSO;
};