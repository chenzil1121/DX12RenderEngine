#pragma once
#include "RenderDevice.h"
#include "Geometry.h"

class ShadowMap
{
public:
	ShadowMap(RenderDevice* pCore, XMFLOAT3 LightPos, XMFLOAT3 LightDirection);

	void Create();

	void UpdateLightVP(XMFLOAT3 LightPos, XMFLOAT3 LightDirection);

	void Render(GraphicsContext& Context, std::vector<std::unique_ptr<Geometry>>& geos);

	Buffer* GetShadowMapCB()
	{
		return ShadowMapCB.get();
	}

	TextureViewer* GetShadowMapSRV()
	{
		return shadowMapSRV.get();
	}
private:
	RenderDevice* pCore;

	std::unique_ptr<RootSignature> ShadowMapRS;
	std::unique_ptr<GraphicsPSO> ShadowMapPSO;

	std::unique_ptr<Texture> shadowMap;
	std::unique_ptr<TextureViewer> shadowMapDSV;
	std::unique_ptr<TextureViewer> shadowMapSRV;

	ShadowMapConstants shadowMapCnstants;
	std::unique_ptr<Buffer> ShadowMapCB;

	DXGI_FORMAT shadowMapFormat = DXGI_FORMAT_R32_TYPELESS;
	UINT shadowMapDim = 2048;
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissor;
};