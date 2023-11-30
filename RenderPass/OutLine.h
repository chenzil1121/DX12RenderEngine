#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "Geometry.h"


class OutLinePass
{
public:
	OutLinePass(RenderDevice* pCore, SwapChain* swapchain);

	void Create();

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, std::vector<std::unique_ptr<Geometry>>& geos);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> OutLinePassRS;
	std::unique_ptr<GraphicsPSO> OutLinePassPSO;
};