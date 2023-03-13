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

	TextureViewer* GetIBLView() { return m_IBLTexView.get(); }
	
private:
	RenderDevice* m_pDevice;

	DXGI_FORMAT m_BrdfLookUpTableFmt = DXGI_FORMAT_R16G16_FLOAT;
	DXGI_FORMAT m_PreIrradianceEnvMapFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
	DXGI_FORMAT m_PreFilterEnvMapFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;

	int m_BrdfLookUpTableDim = 512;
	int m_IrradianceEnvMapDim = 64;
	int m_PrefilteredEnvMapDim = 256;

	int m_IrradianceEnvMapMipNum = 7;
	int m_PrefilteredEnvMapMipNum = 9;

	std::unique_ptr<RootSignature> m_PreBRDFRS;
	std::unique_ptr<RootSignature> m_PreIrradianceEnvMapRS;
	std::unique_ptr<RootSignature> m_PreFilterEnvMapRS;

	std::unique_ptr<GraphicsPSO> m_PreBRDFPSO;
	std::unique_ptr<GraphicsPSO> m_PreIrradianceEnvMapPSO;
	std::unique_ptr<GraphicsPSO> m_PreFilterEnvMapPSO;

	std::unique_ptr<Texture> m_BrdfLookUpTable;
	std::unique_ptr<Texture> m_PreIrradianceEnvMap;
	std::unique_ptr<Texture> m_PreFilterEnvMap;

	std::unique_ptr<TextureViewer>m_IBLTexView;
	TextureViewer* m_EnvMapView;
};