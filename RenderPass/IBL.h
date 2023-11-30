#pragma once
#include"RenderDevice.h"
#include"TextureLoader.h"
#include"Buffer.h"
#include"Sampler.h"
class IBL
{
public:
	IBL(RenderDevice* pDevice, TextureViewer* EnvMapView);
	IBL(RenderDevice* pDevice,
		int BrdfLookUpTableDim,
		int IrradianceEnvMapDim,
		int PrefilteredEnvMapDim,
		DXGI_FORMAT BrdfLookUpTableFmt,
		DXGI_FORMAT PreIrradianceEnvMapFmt,
		DXGI_FORMAT PreFilterEnvMapFmt,
		TextureViewer* EnvMapView
	);

	void PreComputeBRDF();

	void PreComputeCubeMaps();

	void CreateTextureView();

	TextureViewer* GetIBLView() { return IBLTexView.get(); }

private:
	RenderDevice* pCore;

	DXGI_FORMAT brdfLookUpTableFmt = DXGI_FORMAT_R16G16_FLOAT;
	DXGI_FORMAT preIrradianceEnvMapFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
	DXGI_FORMAT preFilterEnvMapFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;

	int brdfLookUpTableDim = 512;
	int irradianceEnvMapDim = 64;
	int prefilteredEnvMapDim = 256;

	int irradianceEnvMapMipNum = 7;
	int prefilteredEnvMapMipNum = 9;

	std::unique_ptr<RootSignature> preBRDFRS;
	std::unique_ptr<RootSignature> preIrradianceEnvMapRS;
	std::unique_ptr<RootSignature> preFilterEnvMapRS;

	std::unique_ptr<GraphicsPSO> preBRDFPSO;
	std::unique_ptr<GraphicsPSO> preIrradianceEnvMapPSO;
	std::unique_ptr<GraphicsPSO> preFilterEnvMapPSO;

	std::unique_ptr<Texture> brdfLookUpTable;
	std::unique_ptr<Texture> preIrradianceEnvMap;
	std::unique_ptr<Texture> preFilterEnvMap;

	std::unique_ptr<TextureViewer>IBLTexView;
	TextureViewer* envMapView;
};