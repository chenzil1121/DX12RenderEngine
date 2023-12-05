#pragma once
#pragma once
#include "RenderDevice.h"
#include "Scene.h"

struct ShadowMapConstants
{
	XMFLOAT4X4 LightMVP;
	float NearZ;
	float FarZ;
};

class VarianceShadowMap
{
public:
	VarianceShadowMap(RenderDevice* pCore, XMFLOAT3 LightPos, XMFLOAT3 LightDirection);

	void Create();

	void UpdateLightVP(XMFLOAT3 LightPos, XMFLOAT3 LightDirection);

	void Render(GraphicsContext& Context, Scene* scene);

	Buffer* GetShadowMapCB()
	{
		return ShadowMapCB.get();
	}

	TextureViewer* GetShadowMapSRV()
	{
		return varianceShadowMapSRV.get();
	}
private:
	RenderDevice* pCore;

	std::unique_ptr<RootSignature> VarianceShadowMapRS;
	std::unique_ptr<GraphicsPSO> VarianceShadowMapPSO;

	std::unique_ptr<Texture> varianceShadowMap;
	std::unique_ptr<Texture> depthBuffer;
	std::unique_ptr<TextureViewer> varianceShadowMapRTV;
	std::unique_ptr<TextureViewer> varianceShadowMapSRV;
	std::unique_ptr<TextureViewer> depthBufferView;

	ShadowMapConstants shadowMapCnstants;
	std::unique_ptr<Buffer> ShadowMapCB;

	DXGI_FORMAT shadowMapFormat = DXGI_FORMAT_R32G32_FLOAT;
	UINT shadowMapDim = 2048;
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissor;
};