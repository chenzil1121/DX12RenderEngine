#pragma once
#include"RenderDevice.h"
#include"SwapChain.h"
#include"Buffer.h"
#include"TextureLoader.h"
#include"Sampler.h"
#include "Geometry.h"

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

class Background
{
public:
	Background(RenderDevice* pCore, SwapChain* pSwapChain, XMVECTORF32 top, XMVECTORF32 buttom);

	void SetBackgroudColor(XMVECTORF32 top, XMVECTORF32 buttom);
	
	void Create();
	
	void GenEnvMap();
	
	void Resize(UINT width, UINT height, float aspectRatio)
	{
		GenEnvMap();
	}

	TextureViewer* GetEnvMapView()
	{
		return backgroudEnvMapSRV.get();
	}

	void Render(GraphicsContext& Context);
private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> backgroudRS;
	std::unique_ptr<GraphicsPSO> backgroudPSO;
	std::unique_ptr<GraphicsPSO> backgroudGenEnvMapXZPSO;
	std::unique_ptr<GraphicsPSO> backgroudGenEnvMapYPSO;

	std::unique_ptr<Buffer> backgroudCBuffer;
	BackgroudConstants backgroudConstants;

	std::unique_ptr<Texture>backgroudEnvMapPositiveX;
	std::unique_ptr<Texture>backgroudEnvMapNegativeX;
	std::unique_ptr<Texture>backgroudEnvMapPositiveY;
	std::unique_ptr<Texture>backgroudEnvMapNegativeY;
	std::unique_ptr<Texture>backgroudEnvMapPositiveZ;
	std::unique_ptr<Texture>backgroudEnvMapNegativeZ;
	std::unique_ptr<Texture>backgroudEnvMap;
	std::unique_ptr<TextureViewer>backgroudEnvMapXZRTV;
	std::unique_ptr<TextureViewer>backgroudEnvMapYRTV;
	std::unique_ptr<TextureViewer>backgroudEnvMapSRV;
};