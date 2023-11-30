#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "Geometry.h"
#include "Material.h"
#include "Sampler.h"
#include "ShadowMap.h"
#include "VarianceShadowMap.h"

#define NUMRT 6

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

	void Render(GraphicsContext& Context, Buffer* PassConstantBuffer, std::vector<std::unique_ptr<Geometry>>& geos, std::map<std::string, PBRMaterial>& pbrMaterials, ShadowMap* shadowMap = nullptr);

	TextureViewer* GetGbufferSRV()
	{
		return GbufferSRV.get();
	};

	TextureViewer* GetPreSRV()
	{
		return PreSRV.get();
	}

	TextureViewer* GetVisibilitySRV()
	{
		return VisSRV.get();
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
	std::unique_ptr<TextureViewer> VisSRV;
	std::unique_ptr<TextureViewer> PreSRV;

	std::array<std::pair<std::wstring, DXGI_FORMAT>, NUMRT> GbufferInfo =
	{
		std::make_pair(L"PositionHit",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"AlbedoRoughness",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"NormalMetallic",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"MotionLinearZ",DXGI_FORMAT_R32G32B32A32_FLOAT),
		std::make_pair(L"ObjectID",DXGI_FORMAT_R32_UINT),
		std::make_pair(L"Visibility",DXGI_FORMAT_R32_FLOAT)
	};

	std::vector<std::array<float, 4>> rtvClearColors =
	{
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f,0.0f},
		{0.0f},
		{1.0f}
	};
};