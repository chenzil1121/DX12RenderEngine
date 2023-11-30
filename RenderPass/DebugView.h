#pragma once
#include"RenderDevice.h"
#include"SwapChain.h"

class DebugView
{
public:
	DebugView(RenderDevice* pCore, SwapChain* pSwapChain);

	void Create();

	void Render(GraphicsContext& Context, TextureViewer* debugSRV);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> DebugViewRS;
	std::unique_ptr<GraphicsPSO> DebugViewPSO;
};