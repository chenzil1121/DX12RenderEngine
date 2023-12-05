#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "Scene.h"
#include "Material.h"
#include "Sampler.h"
#include "VarianceShadowMap.h"

#define NUMRT 5

class Gbuffer 
{
public:
	Gbuffer(RenderDevice* pCore, SwapChain* swapChain);

	void Create();

	void CreateTexture();

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateTexture();
	}

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Scene* scene);

	TextureViewer* GetGbufferSRV()
	{
		return GbufferSRV.get();
	};

	TextureViewer* GetPreSRV()
	{
		return PreSRV.get();
	}

	Texture* GetVisibility()
	{
		return RTs[5].get();
	}
private:

	RenderDevice* pCore;
	SwapChain* pSwapChain;

	std::unique_ptr<RootSignature> GbufferRS;
	std::unique_ptr<GraphicsPSO> GbufferPSO;


	std::unique_ptr<Texture> RTs[NUMRT];
	std::unique_ptr<Texture>PreNormalMetallic;
	std::unique_ptr<Texture>PreMotionLinearZ;
	std::unique_ptr<Texture>PreObjectID;

	std::unique_ptr<TextureViewer> GbufferRTV;
	std::unique_ptr<TextureViewer> GbufferSRV;
	std::unique_ptr<TextureViewer> PreSRV;

	std::array<std::pair<std::wstring, DXGI_FORMAT>, NUMRT> GbufferInfo =
	{
		std::make_pair(L"PositionHit",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"AlbedoRoughness",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"NormalMetallic",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"MotionLinearZ",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"ObjectID",DXGI_FORMAT_R32_UINT),
	};

	std::vector<std::array<float, 4>> rtvClearColors =
	{
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f}
	};
};